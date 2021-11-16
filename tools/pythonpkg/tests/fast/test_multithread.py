import duckdb
import pytest
import threading
import queue as Queue
try:
    import pyarrow as pa
    can_run = True
except:
    can_run = False
class DuckDBThreaded:
    def __init__(self,duckdb_insert_thread_count,thread_function):
        self.duckdb_insert_thread_count = duckdb_insert_thread_count
        self.threads = []
        self.thread_function = thread_function
        
    def multithread_test(self,if_all_true=True):
        duckdb_conn = duckdb.connect()
        queue = Queue.Queue()
        return_value = False

        for i in range(0,self.duckdb_insert_thread_count):
            self.threads.append(threading.Thread(target=self.thread_function, args=(duckdb_conn,queue),name='duckdb_thread_'+str(i)))

        for i in range(0,len(self.threads)):
            self.threads[i].start()
            if not if_all_true:
                if queue.get():
                    return_value = True
            else:
                if i == 0 and queue.get():
                    return_value = True
                elif queue.get() and return_value:
                    return_value = True
            
        for i in range(0,len(self.threads)):
            self.threads[i].join()

        assert (return_value)

def execute_query_same_connection(duckdb_conn, queue):
    try:
        out = duckdb_conn.execute('select i from (values (42), (84), (NULL), (128)) tbl(i)')
        queue.put(False)
    except:
        queue.put(True)

def execute_query(duckdb_conn, queue):
    # Get a new connection
    duckdb_conn = duckdb.connect()
    try:
        duckdb_conn.execute('select i from (values (42), (84), (NULL), (128)) tbl(i)')
        queue.put(True)
    except:
        queue.put(False)  

def execute_many_query(duckdb_conn, queue):
    # Get a new connection
    duckdb_conn = duckdb.connect()
    try:
        # from python docs
        duckdb_conn.execute('''CREATE TABLE stocks
             (date text, trans text, symbol text, qty real, price real)''')
        # Larger example that inserts many records at a time
        purchases = [('2006-03-28', 'BUY', 'IBM', 1000, 45.00),
                     ('2006-04-05', 'BUY', 'MSFT', 1000, 72.00),
                     ('2006-04-06', 'SELL', 'IBM', 500, 53.00),
                    ]
        duckdb_conn.executemany('INSERT INTO stocks VALUES (?,?,?,?,?)', purchases)
        queue.put(True)
    except:
        queue.put(False)  

def fetchone_query(duckdb_conn, queue):
    # Get a new connection
    duckdb_conn = duckdb.connect()
    try:
        duckdb_conn.execute('select i from (values (42), (84), (NULL), (128)) tbl(i)').fetchone()
        queue.put(True)
    except:
        queue.put(False)  

def fetchall_query(duckdb_conn, queue):
    # Get a new connection
    duckdb_conn = duckdb.connect()
    try:
        duckdb_conn.execute('select i from (values (42), (84), (NULL), (128)) tbl(i)').fetchall()
        queue.put(True)
    except:
        queue.put(False)  

def conn_close(duckdb_conn, queue):
    # Get a new connection
    duckdb_conn = duckdb.connect()
    try:
        duckdb_conn.close()
        queue.put(True)
    except:
        queue.put(False)  

def fetchnp_query(duckdb_conn, queue):
    # Get a new connection
    duckdb_conn = duckdb.connect()
    try:
        duckdb_conn.execute('select i from (values (42), (84), (NULL), (128)) tbl(i)').fetchnumpy()
        queue.put(True)
    except:
        queue.put(False) 

def fetchdf_query(duckdb_conn, queue):
    # Get a new connection
    duckdb_conn = duckdb.connect()
    try:
        duckdb_conn.execute('select i from (values (42), (84), (NULL), (128)) tbl(i)').fetchdf()
        queue.put(True)
    except:
        queue.put(False) 

def fetchdf_chunk_query(duckdb_conn, queue):
    # Get a new connection
    duckdb_conn = duckdb.connect()
    try:
        duckdb_conn.execute('select i from (values (42), (84), (NULL), (128)) tbl(i)').fetch_df_chunk()
        queue.put(True)
    except:
        queue.put(False) 

def fetch_arrow_query(duckdb_conn, queue):
    # Get a new connection
    duckdb_conn = duckdb.connect()
    try:
        duckdb_conn.execute('select i from (values (42), (84), (NULL), (128)) tbl(i)').fetch_arrow_table()
        queue.put(True)
    except:
        queue.put(False) 

def fetch_arrow_chunk_query(duckdb_conn, queue):
    # Get a new connection
    duckdb_conn = duckdb.connect()
    try:
        duckdb_conn.execute('select i from (values (42), (84), (NULL), (128)) tbl(i)').fetch_arrow_chunk()
        queue.put(True)
    except:
        queue.put(False) 

def fetch_record_batch_query(duckdb_conn, queue):
    # Get a new connection
    duckdb_conn = duckdb.connect()
    try:
        duckdb_conn.execute('select i from (values (42), (84), (NULL), (128)) tbl(i)').fetch_record_batch()
        queue.put(True)
    except:
        queue.put(False) 

class TestDuckMultithread(object):
    def test_same_conn(self, duckdb_cursor):
        duck_threads = DuckDBThreaded(10,execute_query_same_connection)
        duck_threads.multithread_test(False)

    def test_execute(self, duckdb_cursor):
        duck_threads = DuckDBThreaded(10,execute_query)
        duck_threads.multithread_test()

    def test_execute_many(self, duckdb_cursor):
        duck_threads = DuckDBThreaded(10,execute_many_query)
        duck_threads.multithread_test()

    def test_fetchone(self, duckdb_cursor):
        duck_threads = DuckDBThreaded(10,fetchone_query)
        duck_threads.multithread_test()

    def test_fetchall(self, duckdb_cursor):
        duck_threads = DuckDBThreaded(10,fetchall_query)
        duck_threads.multithread_test()

    def test_close(self, duckdb_cursor):
        duck_threads = DuckDBThreaded(10,conn_close)
        duck_threads.multithread_test()

    def test_fetchnp(self, duckdb_cursor):
        duck_threads = DuckDBThreaded(10,fetchnp_query)
        duck_threads.multithread_test()

    def test_fetchdf(self, duckdb_cursor):
        duck_threads = DuckDBThreaded(10,fetchdf_query)
        duck_threads.multithread_test()

    def test_fetchdfchunk(self, duckdb_cursor):
        duck_threads = DuckDBThreaded(10,fetchdf_chunk_query)
        duck_threads.multithread_test()

    def test_fetcharrow(self, duckdb_cursor):
        duck_threads = DuckDBThreaded(10,fetch_arrow_query)
        duck_threads.multithread_test()

    def test_fetch_arrow_chunk(self, duckdb_cursor):
        duck_threads = DuckDBThreaded(10,fetch_arrow_chunk_query)
        duck_threads.multithread_test()

    def test_fetch_record_batch(self, duckdb_cursor):
        duck_threads = DuckDBThreaded(10,fetch_record_batch_query)
        duck_threads.multithread_test()