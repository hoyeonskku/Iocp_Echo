#pragma once
#include "Windows.h"
#include "CObjectPool.h"

#define QueueSize 5000

enum StackEventType
{
	push,
	pop,
};



template <typename T>
class Node
{
public:
	Node<T>(T value) : _value(value) {}
	~Node<T>() {}

public:
	T _value;
	Node<T>* _pNextValue = nullptr;
};

template <typename T>
class LogClass
{
public:
	StackEventType _type;
	Node<T>* _pCurrent = nullptr;
	Node<T>* _pNext = nullptr;
};

template <typename T>
class LogQueue
{
public:
	LogClass<T> _arr[10000];
	unsigned long long _count;

	void Enqueue(LogClass<T>& logClass)
	{
		unsigned long long index = InterlockedIncrement(&_count) % 10000;
		_arr[index] = logClass;
	}
};

// 상위 17비트를 식별자로 사용
// 노드 메모리풀을 이용해야 크래시 나지 않음.
// value = id(17Bit) + pNode(47Bit)
template <typename T>
class CLockFreeStack
{
public:
	CLockFreeStack() : _pool(0) { }
	~CLockFreeStack() { }

	void Push(T data)
	{
		Node<T>* pNewNode;
		Node<T>* pNewNodeValue;
		Node<T>* pNewNodeNextValue;
		pNewNode = _pool.Alloc();
		pNewNode->_value = data;
		pNewNodeValue = (Node<T>*)MAKE_VALUE(_id, pNewNode);
		do
		{
			pNewNodeNextValue = _pTopNodeValue;
			pNewNode->_pNextValue = pNewNodeNextValue;
		}
		// top이 저장한 값과 같은 경우에만 Push, 노드를 새로운 top으로 변경
		while ((Node<T>*) InterlockedCompareExchange((unsigned long long*) & _pTopNodeValue, (unsigned long long) pNewNodeValue, (unsigned long long) pNewNodeNextValue) != pNewNodeNextValue);

		//LogClass<T> logClass;
		//logClass._pCurrent = (Node<T>*)MAKE_NODE(pNewNodeValue);
		//logClass._pNext = (Node<T>*)MAKE_NODE(pNewNodeNextValue);
		//logClass._type = StackEventType::push;
		//_logQueue.Enqueue(logClass);
	}

	bool Pop(T& data)
	{
		Node<T>* pReleaseNode;
		Node<T>* pReleaseNodeValue;
		Node<T>* pReleaseNodeNextValue;
		do
		{
			pReleaseNodeValue = _pTopNodeValue;
			pReleaseNode = (Node<T>*) MAKE_NODE(pReleaseNodeValue);
			if (pReleaseNodeValue == nullptr)
				DebugBreak();
			pReleaseNodeNextValue = pReleaseNode->_pNextValue;
		}
		// top이 제거하려는 노드인 경우에만 Pop, next를 새로운 top으로 변경
		while ((Node<T>*)InterlockedCompareExchange((unsigned long long*) & _pTopNodeValue, (unsigned long long) pReleaseNodeNextValue, (unsigned long long)pReleaseNodeValue) != pReleaseNodeValue);
		data = pReleaseNode->_value;
		//LogClass<T> logClass;
		//logClass._pCurrent = (Node<T>*) MAKE_NODE(pReleaseNodeValue);
		//logClass._pNext = (Node<T>*)MAKE_NODE(pReleaseNodeNextValue);
		//logClass._type = StackEventType::pop;
		//_logQueue.Enqueue(logClass);

		_pool.Free(pReleaseNode);
		return true;
	}

private:
	CMemoryPool<Node<T>> _pool;
	Node<T>* _pTopNodeValue = nullptr;
	unsigned long long _id = 0;
	LogQueue<T> _logQueue;
};

