#include "duckdb/main/buffered_data/batched_buffered_data.hpp"
#include "duckdb/common/printer.hpp"
#include "duckdb/main/client_context.hpp"
#include "duckdb/main/stream_query_result.hpp"
#include "duckdb/common/helper.hpp"
#include "duckdb/execution/operator/helper/physical_buffered_batch_collector.hpp"
#include "duckdb/common/stack.hpp"

namespace duckdb {

void BatchedBufferedData::BlockSink(BlockedSink blocked_sink, idx_t batch) {
	lock_guard<mutex> lock(glock);
	D_ASSERT(!blocked_sinks.count(batch));
	blocked_sinks.emplace(std::make_pair(batch, blocked_sink));
}

BatchedBufferedData::BatchedBufferedData(weak_ptr<ClientContext> context)
    : BufferedData(BufferedData::Type::BATCHED, std::move(context)), other_batches_tuple_count(0),
      current_batch_tuple_count(0), min_batch(0) {
}

bool BatchedBufferedData::ShouldBlockBatch(idx_t batch) {
	lock_guard<mutex> lock(glock);
	// If a batch index is specified we need to check only one of the two tuple_counts
	bool is_minimum = IsMinBatch(lock, batch);
	if (is_minimum) {
		return current_batch_tuple_count >= CURRENT_BATCH_BUFFER_SIZE;
	}
	return other_batches_tuple_count >= OTHER_BATCHES_BUFFER_SIZE;
}

bool BatchedBufferedData::BufferIsFull() {
	lock_guard<mutex> lock(glock);
	bool min_filled = current_batch_tuple_count >= CURRENT_BATCH_BUFFER_SIZE;
	bool others_filled = other_batches_tuple_count >= OTHER_BATCHES_BUFFER_SIZE;
	if (batches.empty()) {
		// If there is no batch to scan, we can't break out of the loop
		// Once the execution is properly finished, we'll break out through a different condition
		return false;
	}
	return min_filled || others_filled;
}

bool BatchedBufferedData::IsMinBatch(lock_guard<mutex> &guard, idx_t batch) {
	return min_batch == batch;
}

void BatchedBufferedData::UnblockSinks() {
	lock_guard<mutex> lock(glock);
	stack<idx_t> to_remove;
	for (auto it = blocked_sinks.begin(); it != blocked_sinks.end(); it++) {
		auto batch = it->first;
		auto &blocked_sink = it->second;
		const bool is_minimum = IsMinBatch(lock, batch);
		if (is_minimum) {
			if (current_batch_tuple_count >= CURRENT_BATCH_BUFFER_SIZE) {
				continue;
			}
		} else {
			if (other_batches_tuple_count >= OTHER_BATCHES_BUFFER_SIZE) {
				continue;
			}
		}
		blocked_sink.state.Callback();
		to_remove.push(batch);
	}
	while (!to_remove.empty()) {
		auto batch = to_remove.top();
		to_remove.pop();
		blocked_sinks.erase(batch);
	}
}

void BatchedBufferedData::UpdateMinBatchIndex(idx_t min_batch_index) {
	lock_guard<mutex> lock(glock);
	min_batch = MaxValue(min_batch, min_batch_index);
	// printf("UPDATE MIN BATCH: min_batch: %llu\n", min_batch);
	// printf("current-batch tuple_count: %llu\n", current_batch_tuple_count.load());
	// printf("other-batches tuple_count: %llu\n\n", other_batches_tuple_count.load());

	stack<idx_t> to_remove;
	for (auto &it : in_progress_batches) {
		auto batch = it.first;
		auto &buffered_chunks = it.second;
		if (batch > min_batch) {
			// This batch is still in progress, can not be fetched from yet
			break;
		}
		if (batch != min_batch && !buffered_chunks.completed) {
			// We haven't completed this batch yet
			break;
		}
		D_ASSERT(buffered_chunks.completed || batch == min_batch);

		// We have already materialized chunks, have to move them to `batches` so they can be scanned
		auto &chunks = buffered_chunks.chunks;

		idx_t tuple_count = 0;
		for (auto it = chunks.begin(); it != chunks.end(); it++) {
			auto chunk = std::move(*it);
			tuple_count += chunk->size();
			batches.push_back(std::move(chunk));
		}
		other_batches_tuple_count -= tuple_count;
		current_batch_tuple_count += tuple_count;
		to_remove.push(batch);
	}
	while (!to_remove.empty()) {
		auto batch = to_remove.top();
		to_remove.pop();
		in_progress_batches.erase(batch);
	}
}

PendingExecutionResult BatchedBufferedData::ReplenishBuffer(StreamQueryResult &result,
                                                            ClientContextLock &context_lock) {
	if (Closed()) {
		return PendingExecutionResult::EXECUTION_ERROR;
	}
	if (BufferIsFull()) {
		// The buffer isn't empty yet, just return
		return PendingExecutionResult::RESULT_READY;
	}
	UnblockSinks();
	// Let the executor run until the buffer is no longer empty
	auto cc = context.lock();
	auto res = cc->ExecuteTaskInternal(context_lock, result);
	while (!PendingQueryResult::IsFinished(res)) {
		if (BufferIsFull()) {
			break;
		}
		// Check if we need to unblock more sinks to reach the buffer size
		UnblockSinks();
		res = cc->ExecuteTaskInternal(context_lock, result);
	}
	// printf("IS_FINISHED\n");
	return res;
}

void BatchedBufferedData::CompleteBatch(idx_t batch) {
	auto it = in_progress_batches.find(batch);
	if (it == in_progress_batches.end()) {
		return;
	}

	auto &buffered_chunks = it->second;
	buffered_chunks.completed = true;
}

unique_ptr<DataChunk> BatchedBufferedData::Scan() {
	unique_ptr<DataChunk> chunk;
	bool empty = false;
	{
		lock_guard<mutex> lock(glock);
		if (!batches.empty()) {
			chunk = std::move(batches.front());
			batches.pop_front();
		} else {
			empty = true;
		}
	}
	if (empty) {
		D_ASSERT(!chunk);
		// Increase the min batch index to see if there are still completed batches
		// waiting to be processed
		auto it = in_progress_batches.begin();
		if (it != in_progress_batches.end()) {
			auto batch = it->first;
			auto &chunks = it->second;
			if (chunks.completed) {
				// By updating the min batch index, we'll move the chunks to 'batches';
				UpdateMinBatchIndex(batch);
			}
			lock_guard<mutex> lock(glock);
			if (!batches.empty()) {
				chunk = std::move(batches.front());
				batches.pop_front();
			} else {
				empty = true;
			}
		}
	}

	if (!chunk) {
		context.reset();
		D_ASSERT(blocked_sinks.empty());
		D_ASSERT(in_progress_batches.empty());
		return nullptr;
	}

	current_batch_tuple_count -= chunk->size();

	// printf("SCAN: min_batch: %llu\n", min_batch);
	// printf("current-batch tuple_count: %llu\n", current_batch_tuple_count.load());
	// printf("other-batches tuple_count: %llu\n\n", other_batches_tuple_count.load());

	return chunk;
}

void BatchedBufferedData::Append(unique_ptr<DataChunk> chunk, idx_t batch) {
	// We should never find any chunks with a smaller batch index than the minimum
	D_ASSERT(batch >= min_batch);
	lock_guard<mutex> lock(glock);
	if (batch == min_batch) {
		current_batch_tuple_count += chunk->size();
		batches.push_back(std::move(chunk));
	} else {
		auto &buffered_chunks = in_progress_batches[batch];
		auto &chunks = buffered_chunks.chunks;
		buffered_chunks.completed = false;

		other_batches_tuple_count += chunk->size();
		chunks.push_back(std::move(chunk));
	}
	// printf("APPEND: batch: %llu | min_batch: %llu\n", batch, min_batch);
	// printf("current-batch tuple_count: %llu\n", current_batch_tuple_count.load());
	// printf("other-batches tuple_count: %llu\n\n", other_batches_tuple_count.load());
}

} // namespace duckdb
