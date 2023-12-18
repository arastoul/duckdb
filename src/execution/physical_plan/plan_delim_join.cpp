#include "duckdb/common/enum_util.hpp"
#include "duckdb/execution/operator/aggregate/physical_hash_aggregate.hpp"
#include "duckdb/execution/operator/join/physical_hash_join.hpp"
#include "duckdb/execution/operator/join/physical_left_delim_join.hpp"
#include "duckdb/execution/operator/join/physical_right_delim_join.hpp"
#include "duckdb/execution/operator/projection/physical_projection.hpp"
#include "duckdb/execution/physical_plan_generator.hpp"
#include "duckdb/planner/expression/bound_reference_expression.hpp"

namespace duckdb {

static void GatherDelimScans(const PhysicalOperator &op, vector<const_reference<PhysicalOperator>> &delim_scans) {
	if (op.type == PhysicalOperatorType::DELIM_SCAN) {
		delim_scans.push_back(op);
	}
	for (auto &child : op.children) {
		GatherDelimScans(*child, delim_scans);
	}
}

unique_ptr<PhysicalOperator> PhysicalPlanGenerator::PlanDelimJoin(LogicalComparisonJoin &op) {
	// Try to swap LHS and RHS
	switch (op.join_type) {
	case JoinType::SINGLE:
	case JoinType::MARK:
		// We can't flip these joins (yet)
		return PlanLeftDelimJoin(op);
	case JoinType::INNER:
	case JoinType::OUTER:
		// These are symmetric
		LogicalJoin::FlipChildren(op, op.join_type);
		break;
	case JoinType::LEFT:
		LogicalJoin::FlipChildren(op, JoinType::RIGHT);
		break;
	case JoinType::RIGHT:
		LogicalJoin::FlipChildren(op, JoinType::LEFT);
		break;
	case JoinType::SEMI:
		LogicalJoin::FlipChildren(op, JoinType::RIGHT_SEMI);
		break;
	case JoinType::ANTI:
		LogicalJoin::FlipChildren(op, JoinType::RIGHT_ANTI);
		break;
	default:
		throw NotImplementedException("PhysicalPlanGenerator::PlanDelimJoin for JoinType::%s",
		                              EnumUtil::ToString(op.join_type));
	}

	return PlanRightDelimJoin(op);
}

unique_ptr<PhysicalOperator> PhysicalPlanGenerator::PlanLeftDelimJoin(LogicalComparisonJoin &op) {
	// first create the underlying join
	auto plan = PlanComparisonJoin(op);
	// this should create a join, not a cross product
	D_ASSERT(plan && plan->type != PhysicalOperatorType::CROSS_PRODUCT);
	// duplicate eliminated join
	// first gather the scans on the duplicate eliminated data set from the RHS
	vector<const_reference<PhysicalOperator>> delim_scans;
	GatherDelimScans(*plan->children[1], delim_scans);
	if (delim_scans.empty()) {
		// no duplicate eliminated scans in the RHS!
		// in this case we don't need to create a delim join
		// just push the normal join
		return plan;
	}
	vector<LogicalType> delim_types;
	vector<unique_ptr<Expression>> distinct_groups, distinct_expressions;
	for (auto &delim_expr : op.duplicate_eliminated_columns) {
		D_ASSERT(delim_expr->type == ExpressionType::BOUND_REF);
		auto &bound_ref = delim_expr->Cast<BoundReferenceExpression>();
		delim_types.push_back(bound_ref.return_type);
		distinct_groups.push_back(make_uniq<BoundReferenceExpression>(bound_ref.return_type, bound_ref.index));
	}
	// now create the duplicate eliminated join
	auto delim_join =
	    make_uniq<PhysicalLeftDelimJoin>(op.types, std::move(plan), delim_scans, op.estimated_cardinality);
	// we still have to create the DISTINCT clause that is used to generate the duplicate eliminated chunk
	delim_join->distinct = make_uniq<PhysicalHashAggregate>(context, delim_types, std::move(distinct_expressions),
	                                                        std::move(distinct_groups), op.estimated_cardinality);
	return std::move(delim_join);
}

unique_ptr<PhysicalOperator> PhysicalPlanGenerator::PlanRightDelimJoin(LogicalComparisonJoin &op) {
	// first create the underlying join
	auto plan = PlanComparisonJoin(op);
	// this should create a join, not a cross product
	D_ASSERT(plan && plan->type != PhysicalOperatorType::CROSS_PRODUCT);
	// duplicate eliminated join
	// first gather the scans on the duplicate eliminated data set from the LHS
	vector<const_reference<PhysicalOperator>> delim_scans;
	GatherDelimScans(*plan->children[0], delim_scans);
	if (delim_scans.empty()) {
		// no duplicate eliminated scans in the RHS!
		// in this case we don't need to create a delim join
		// just push the normal join
		return plan;
	}
	vector<LogicalType> delim_types;
	vector<unique_ptr<Expression>> distinct_groups, distinct_expressions;
	for (auto &delim_expr : op.duplicate_eliminated_columns) {
		D_ASSERT(delim_expr->type == ExpressionType::BOUND_REF);
		auto &bound_ref = delim_expr->Cast<BoundReferenceExpression>();
		delim_types.push_back(bound_ref.return_type);
		distinct_groups.push_back(make_uniq<BoundReferenceExpression>(bound_ref.return_type, bound_ref.index));
	}
	// now create the duplicate eliminated join
	auto delim_join =
	    make_uniq<PhysicalRightDelimJoin>(op.types, std::move(plan), delim_scans, op.estimated_cardinality);
	// we still have to create the DISTINCT clause that is used to generate the duplicate eliminated chunk
	delim_join->distinct = make_uniq<PhysicalHashAggregate>(context, delim_types, std::move(distinct_expressions),
	                                                        std::move(distinct_groups), op.estimated_cardinality);
	return std::move(delim_join);
}

} // namespace duckdb
