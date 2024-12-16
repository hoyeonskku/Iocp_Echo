#include <Windows.h>
#include "CObjectPool.h"

template <typename T>
struct QueueNode
{
    T _data;
    QueueNode<T>* _next;
};

template <typename T>
class CLockFreeQueue
{
public:
    long _size;
    CMemoryPool<QueueNode<T>, true> _pool;

    unsigned long long _headValue;        // ���۳�带 ����Ʈ�Ѵ�.
    unsigned long long _tailValue;        // ��������带 ����Ʈ�Ѵ�.
    unsigned long long _headId = 0;
    unsigned long long _tailId = 0;


public:
    CLockFreeQueue() :_pool(0)
    {
        _size = 0;
        QueueNode<T>* dummy = _pool.Alloc();
        dummy->_next = NULL;
        _headValue = MAKE_VALUE(_headId, dummy);
        _tailValue = MAKE_VALUE(_tailId, dummy);
    }

    void Enqueue(T t)
    {
        QueueNode<T>* node = _pool.Alloc();
        node->_data = t;
        node->_next = NULL;

        unsigned long long nodeValue = MAKE_VALUE(_tailId, node);
        unsigned long long tailValue;
        QueueNode<T>* pTail;
        QueueNode<T>* pNext;

        while (true)
        {
            tailValue = _tailValue;
            pTail = (QueueNode<T>*) MAKE_NODE(tailValue);
            pNext = pTail->_next;

            // 2�� cas ������ ��츦 ���� tail�� �Ű��� �� ������ �Ű���.
            if (pNext != NULL)
            {
                unsigned long long nextValue = MAKE_VALUE(_tailId, pNext);
                InterlockedCompareExchangePointer((PVOID*)&_tailValue, (PVOID)nextValue, (PVOID)tailValue);
                continue;
            }

            // cas 1
            if (InterlockedCompareExchangePointer((PVOID*)&pTail->_next, (PVOID)node, NULL) == pNext)
            {
                // cas 2
                InterlockedCompareExchangePointer((PVOID*)&_tailValue, (PVOID)nodeValue, (PVOID)tailValue);
                break;
            }
        }

        InterlockedExchangeAdd(&_size, 1);
    }

    int Dequeue(T& t)
    {
        int size = InterlockedDecrement(&_size);
        if (size < 0)
        {
            InterlockedIncrement(&_size);
            DebugBreak();
            return -1;
        }
        QueueNode<T>* pHead;
        unsigned long long headValue;
        QueueNode<T>* pTail;
        unsigned long long tailValue;
        QueueNode<T>* pHeadNext;
        unsigned long long headNextValue;
        QueueNode<T>* pTailNext;
        unsigned long long tailNextValue;
        while (true)
        {
            headValue = _headValue;
            tailValue = _tailValue;
            pHead = (QueueNode<T>*) MAKE_NODE(headValue);
            pTail = (QueueNode<T>*) MAKE_NODE(tailValue);

            pHeadNext = pHead->_next;
            if (pHeadNext == NULL)
                continue;

            // head�� tail�� ���� ���, head�� tail�� �������� �ʵ��� ��������� ��.
            if (pHead == pTail)
            {
                pTailNext = pTail->_next;
                if (pTailNext == NULL)
                    continue;

                tailNextValue = MAKE_NODE(pTailNext);
                InterlockedCompareExchangePointer((PVOID*)&_tailValue, (PVOID)tailNextValue, (PVOID)tailValue);
                continue;
            }
            else
            {
                headNextValue = MAKE_VALUE(_headId, pHeadNext);
                t = pHeadNext->_data;
                if ((unsigned long long)(InterlockedCompareExchangePointer((volatile PVOID*)&_headValue, (PVOID)headNextValue, (PVOID)headValue)) == headValue)
                {
                    _pool.Free(pHead);
                    break;
                }
            }
        }
        return 0;
    }

    long GetSize() { return _size; }
};