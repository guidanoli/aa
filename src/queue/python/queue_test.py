from pyqueue import Queue

def test_empty():
	q = Queue()
	assert q.is_empty()

def test_queue_one():
	q = Queue()
	q.queue(1)
	assert not q.is_empty()
	assert q.dequeue() == 1
	assert q.is_empty()

def test_queue_none():
	q = Queue()
	q.queue(None)
	assert not q.is_empty()
	assert q.dequeue() is None
	assert q.is_empty()

def test_queue_more():
	q = Queue()
	q.queue(1)
	q.queue('true')
	q.queue(True)
	assert q.dequeue() == 1
	assert q.dequeue() == 'true'
	assert q.dequeue() == True
	assert q.is_empty()

def test_iter():
	q = Queue()
	list = [i for i in range(10)]
	for i in list:
		q.queue(i)
	new_list = [i for i in q]
	assert q.is_empty()
	assert list == new_list

def test_iter2():
	q = Queue()
	r = Queue()
	list = [i for i in range(10)]
	for i in list:
		r.queue(i)
	assert not r.is_empty()
	for i in r:
		q.queue(i)
	assert r.is_empty()
	assert not q.is_empty()
	new_list = [i for i in q]
	assert q.is_empty()
	assert list == new_list