#ifndef  __MEMORY_POOL__
#define  __MEMORY_POOL__
#pragma once


#include <new.h>
#include <iostream>
#include <stack>
#include <assert.h>
#include <Windows.h>
#define MAKE_NODE(value) ((unsigned long long)value & 0x7FFFFFFFFFFF)
#define MAKE_VALUE(id, node) ((InterlockedIncrement(&id) << 47) | (unsigned long long) node)
#define EXTRACT_ID(value) (unsigned long long)value >> 47;

// 노드를 생성 삭제하는게 아님
// 노드에 대한 추가적인 할당 해제 없게 만들었음
// 메모리풀은 노드를 떼서 버려버렸음. 
// 리턴타입이 포인터니깐 이걸 붙여서 줬음. 밸류타입이었으면 불가능했음.
// 풀은 우리가 리턴타입이 포인터로 만들었고, 스택은 리턴타입이 탬플릿 타입이기 때문에 이게 가능해졌다.
// 그렇기에 디커밋에 대한 것이 해결이 된 것이다.

template <class DATA, bool bPlacementNew = false>
class CMemoryPool
{
public:
	struct st_BLOCK_NODE
	{
#ifdef _DEBUG
		st_BLOCK_NODE* _pNext;          // 다음 블럭 노드 포인터
		CMemoryPool<DATA, bPlacementNew>* pParent;        // 부모 블럭 노드 포인터
		char guardBefore[8];
		DATA _data;                       // T 타입의 데이터
		char guardAfter[8];
#endif
#ifndef _DEBUG
		st_BLOCK_NODE* _pNextValue;          // 다음 블럭 노드 포인터
		DATA _data;                       // T 타입의 데이터
#endif
	};
public:
#ifdef _DEBUG		
	static const long long GUARD_VALUE = 0xEAEAEAEAEAEAEAEA;

#endif
	//////////////////////////////////////////////////////////////////////////
	// 생성자, 파괴자.
	//
	// Parameters:	(int) 초기 블럭 개수.
	//				(bool) Alloc 시 생성자 / Free 시 파괴자 호출 여부
	// Return:
	//////////////////////////////////////////////////////////////////////////
	CMemoryPool(int iBlockNum) : _pTopNodeValue(nullptr)
#ifdef _DEBUG
		, m_iUseCount(0), m_iCapacity(iBlockNum)
#endif
	{

#ifdef _DEBUG
		// 블록 초기화
		st_BLOCK_NODE* newNode;
		for (int i = 0; i < iBlockNum; i++)
		{
			newNode = (st_BLOCK_NODE*)malloc(sizeof(st_BLOCK_NODE));
			if constexpr (bPlacementNew)
			{
				new ((char*)newNode + offsetof(st_BLOCK_NODE, _data)) DATA();
			}
			newNode->pParent = this;
			newNode->_pNext = _pTopNodeValue;

			_pTopNodeValue = (st_BLOCK_NODE*)((unsigned long long) newNode | ((_id++) << 47));

			*((long long*)newNode->guardBefore) = GUARD_VALUE;
			*((long long*)newNode->guardAfter) = GUARD_VALUE;
			m_iCapacity++;
			m_iUseCount++;
		}
#endif

#ifndef _DEBUG
		// 블록 초기화
		st_BLOCK_NODE* newNode = nullptr;
		st_BLOCK_NODE* nextNode = nullptr;
		for (int i = 0; i < iBlockNum; i++)
		{
			newNode = (st_BLOCK_NODE*)malloc(sizeof(st_BLOCK_NODE));

			if constexpr (bPlacementNew)
			{
				new ((char*)newNode + offsetof(st_BLOCK_NODE, _data)) DATA();
			}
			newNode->_pNextValue = (st_BLOCK_NODE*)MAKE_VALUE(_id, nextNode);
			nextNode = newNode;
		}
#endif
	};

	~CMemoryPool()
	{
		st_BLOCK_NODE* topNode = (st_BLOCK_NODE*)MAKE_NODE(_pTopNodeValue);
		while (topNode)
		{
			st_BLOCK_NODE* temp = topNode;

			topNode = (st_BLOCK_NODE*)MAKE_NODE(temp->_pNextValue);
			if constexpr (bPlacementNew)
			{
				temp->_data.~DATA();  // 메모리 해제
			}

			delete (st_BLOCK_NODE*)((unsigned long long) temp);
		}
	}
	//////////////////////////////////////////////////////////////////////////
	// 블럭 하나를 할당받는다.  
	//
	// Parameters: 없음.
	// Return: (DATA *) 데이타 블럭 포인터.
	//////////////////////////////////////////////////////////////////////////
	DATA* Alloc(void)
	{
#ifdef _DEBUG

		st_BLOCK_NODE* pReleaseNode;
		st_BLOCK_NODE* pReleaseNodeValue;
		if (_pTopNodeValue == nullptr)
		{
			pReleaseNode = (st_BLOCK_NODE*)malloc(sizeof(st_BLOCK_NODE));
			pReleaseNode->pParent = this;
			*((long long*)pReleaseNode->guardBefore) = GUARD_VALUE;
			*((long long*)pReleaseNode->guardAfter) = GUARD_VALUE;
			if constexpr (bPlacementNew == true)
			{
				new ((char*)pReleaseNode + offsetof(st_BLOCK_NODE, _data)) DATA;
			}
			return (DATA*)((char*)pReleaseNode + offsetof(st_BLOCK_NODE, _data));
		}
		do
		{
			pReleaseNodeValue = _pTopNodeValue;
			pReleaseNode = (st_BLOCK_NODE*)((unsigned long long)pReleaseNodeValue & (unsigned long long) 0x7FFFFFFFFFFF);
			/*if (!Release)
				return;*/
		}
		// top이 제거하려는 노드인 경우에만 Pop, next를 새로운 top으로 변경
		while ((st_BLOCK_NODE*)InterlockedCompareExchange((unsigned long long*) & _pTopNodeValue, (unsigned long long) pReleaseNode->_pNext, (unsigned long long)pReleaseNodeValue) != pReleaseNodeValue);

		m_iCapacity++;
		m_iUseCount++;
		return (DATA*)((unsigned long long)((char*)pReleaseNode + offsetof(st_BLOCK_NODE, _data)));
#endif
#ifndef _DEBUG
		st_BLOCK_NODE* pReleaseNode;
		st_BLOCK_NODE* pReleaseNodeValue;
		st_BLOCK_NODE* pReleaseNextNodeValue;
		do
		{
			pReleaseNodeValue = _pTopNodeValue;
			pReleaseNode = (st_BLOCK_NODE*)MAKE_NODE(pReleaseNodeValue);
			if (pReleaseNode == nullptr)
			{
				pReleaseNode = (st_BLOCK_NODE*)malloc(sizeof(st_BLOCK_NODE));
				if constexpr (bPlacementNew == true)
				{
					new ((char*)pReleaseNode + offsetof(st_BLOCK_NODE, _data)) DATA;
				}
				return (DATA*)((char*)pReleaseNode + offsetof(st_BLOCK_NODE, _data));
			}
			pReleaseNextNodeValue = pReleaseNode->_pNextValue;
			/*if (!Release)
				return;*/
		}
		// top이 제거하려는 노드인 경우에만 Pop, next를 새로운 top으로 변경
		while ((st_BLOCK_NODE*)InterlockedCompareExchange((unsigned long long*) & _pTopNodeValue, (unsigned long long) pReleaseNextNodeValue, (unsigned long long)pReleaseNodeValue) != pReleaseNodeValue);
		return (DATA*)((unsigned long long)((char*)pReleaseNode + offsetof(st_BLOCK_NODE, _data)));
#endif
	};

	//////////////////////////////////////////////////////////////////////////
	// 사용중이던 블럭을 해제한다.
	//
	// Parameters: (DATA *) 블럭 포인터.
	// Return: (BOOL) TRUE, FALSE.
	//////////////////////////////////////////////////////////////////////////
	bool	Free(DATA* pData)
	{
#ifdef _DEBUG			

		st_BLOCK_NODE* pBlockNode = (st_BLOCK_NODE*)((char*)pData - offsetof(st_BLOCK_NODE, _data));
		st_BLOCK_NODE* pBlockValue = (st_BLOCK_NODE*)((unsigned long long)pBlockNode | (InterlockedIncrement(&_id) << 47));
		if (*(long long*)(pBlockNode->guardBefore) != GUARD_VALUE || *(long long*)(pBlockNode->guardAfter) != GUARD_VALUE)
		{
			// 가드 값이 손상되었음을 보고하거나 처리
			std::cerr << "Memory corruption detected during Free!" << std::endl;
			//				std::abort();
			return false; // 또는 적절한 오류 처리를 추가
		}
		if (pBlockNode->pParent != this)
		{
			return false;
		}
		if constexpr (bPlacementNew == true)
		{
			pBlockNode->_data.~DATA();
		}
		do
		{
			pBlockNode->_pNext = _pTopNodeValue;
			pBlockNextValue = pBlockNode->_pNext;
		}
		// top이 저장한 값과 같은 경우에만 Push, 노드를 새로운 top으로 변경
		while ((st_BLOCK_NODE*)InterlockedCompareExchange((unsigned long long*) & _pTopNodeValue, (unsigned long long) pBlockValue, (unsigned long long) pBlockNextValue) != pBlockNextValue);
		m_iUseCount--;
		return true;


#endif
#ifndef _DEBUG
		st_BLOCK_NODE* pBlockNode = (st_BLOCK_NODE*)((char*)pData - offsetof(st_BLOCK_NODE, _data));
		st_BLOCK_NODE* pBlockValue = (st_BLOCK_NODE*)MAKE_VALUE(_id, pBlockNode);
		st_BLOCK_NODE* pBlockNextValue;
		if constexpr (bPlacementNew == true)
		{
			pBlockNode->_data.~DATA();
		}
		do
		{
			pBlockNextValue = _pTopNodeValue;
			pBlockNode->_pNextValue = pBlockNextValue;
		}
		// top이 저장한 값과 같은 경우에만 Push, 노드를 새로운 top으로 변경
		while ((st_BLOCK_NODE*)InterlockedCompareExchange((unsigned long long*) & _pTopNodeValue, (unsigned long long) pBlockValue, (unsigned long long) pBlockNextValue) != pBlockNextValue);
		return true;
#endif
	};
#ifdef _DEBUG
	//////////////////////////////////////////////////////////////////////////
	// 현재 확보 된 블럭 개수를 얻는다. (메모리풀 내부의 전체 개수)
	//
	// Parameters: 없음.
	// Return: (int) 메모리 풀 내부 전체 개수
	//////////////////////////////////////////////////////////////////////////
	int		GetCapacityCount(void) { return m_iCapacity; }

	//////////////////////////////////////////////////////////////////////////
	// 현재 사용중인 블럭 개수를 얻는다.
	//
	// Parameters: 없음.
	// Return: (int) 사용중인 블럭 개수.
	//////////////////////////////////////////////////////////////////////////
	int		GetUseCount(void) { return m_iUseCount; }
#endif

private:
	st_BLOCK_NODE* _pTopNodeValue = nullptr;
	unsigned long long _id = 0;
#ifdef _DEBUG
	unsigned int m_iUseCount;
	unsigned int m_iCapacity;
#endif
};

#endif