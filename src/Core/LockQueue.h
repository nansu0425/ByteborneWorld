#pragma once

#include <deque>
#include <mutex>

namespace core
{
    template<typename T>
    class LockQueue
    {
    public:
        // 기본 생성자
        LockQueue() = default;
        
        // 복사 생성자 삭제 (뮤텍스 때문에)
        LockQueue(const LockQueue&) = delete;
        LockQueue& operator=(const LockQueue&) = delete;
        
        // 이동 생성자와 이동 대입 연산자도 삭제 (뮤텍스 때문에)
        LockQueue(LockQueue&&) = delete;
        LockQueue& operator=(LockQueue&&) = delete;

        // 큐에 요소 추가 (복사)
        void push(const T& item)
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_queue.push_back(item);
        }

        // 큐에 요소 추가 (이동)
        void push(T&& item)
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_queue.push_back(std::move(item));
        }

        // 큐에서 요소 제거 및 반환
        // 성공 시 true, 큐가 비어있으면 false 반환
        // 제거된 요소는 item 참조 파라미터로 전달
        bool pop(T& item)
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (m_queue.empty())
            {
                return false;
            }
            
            item = std::move(m_queue.front());
            m_queue.pop_front();
            return true;
        }

        // 큐가 비어있는지 확인
        bool isEmpty() const
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            return m_queue.empty();
        }

        // 큐의 크기 반환
        size_t size() const
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            return m_queue.size();
        }

        // 큐를 비움
        void clear()
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_queue.clear();
        }

    private:
        mutable std::mutex m_mutex;
        std::deque<T> m_queue;
    };
}
