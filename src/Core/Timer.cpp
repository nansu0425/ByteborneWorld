#include "Timer.h"

namespace core
{
    Timer::Timer()
        : m_nextTimerId(1) // 0은 무효한 ID로 사용
    {
    }

    TimerId Timer::scheduleOnce(Duration delay, TimerCallback callback)
    {
        auto executeTime = std::chrono::steady_clock::now() + delay;
        return scheduleInternal(executeTime, std::move(callback));
    }

    TimerId Timer::scheduleRepeating(Duration delay, Duration interval, TimerCallback callback)
    {
        auto executeTime = std::chrono::steady_clock::now() + delay;
        return scheduleInternal(executeTime, std::move(callback), true, interval);
    }

    TimerId Timer::scheduleAt(TimePoint executeTime, TimerCallback callback)
    {
        return scheduleInternal(executeTime, std::move(callback));
    }

    bool Timer::cancel(TimerId timerId)
    {
        if (timerId == 0)
        {
            return false;
        }

        // 취소된 타이머 목록에 추가
        // 실제 제거는 update()에서 처리
        m_cancelledTimers.insert(timerId);
        return true;
    }

    size_t Timer::update()
    {
        size_t processedCount = 0;
        auto now = std::chrono::steady_clock::now();

        // 실행 시간이 된 타이머들을 처리
        while (!m_timerQueue.empty())
        {
            auto task = m_timerQueue.top();
            
            // 아직 실행 시간이 안 된 경우 중단
            if (task->executeTime > now)
            {
                break;
            }

            m_timerQueue.pop();

            // 취소된 타이머인지 확인
            if (m_cancelledTimers.find(task->id) != m_cancelledTimers.end())
            {
                m_cancelledTimers.erase(task->id);
                continue;
            }

            // 콜백 실행
            if (task->callback)
            {
                task->callback();
                ++processedCount;
            }

            // 반복 타이머인 경우 다시 스케줄링
            if (task->isRepeating && task->interval > Duration::zero())
            {
                task->executeTime += task->interval;
                task->id = generateNextId(); // 새로운 ID 할당 (취소 방지)
                m_timerQueue.push(task);
            }
        }

        return processedCount;
    }

    size_t Timer::getTimerCount() const
    {
        // 취소된 타이머를 제외한 실제 타이머 개수
        return m_timerQueue.size() - m_cancelledTimers.size();
    }

    void Timer::clear()
    {
        // 모든 타이머와 취소 목록 제거
        while (!m_timerQueue.empty())
        {
            m_timerQueue.pop();
        }
        m_cancelledTimers.clear();
    }

    TimerId Timer::generateNextId()
    {
        return m_nextTimerId.fetch_add(1, std::memory_order_relaxed);
    }

    TimerId Timer::scheduleInternal(TimePoint executeTime, TimerCallback callback, bool isRepeating, Duration interval)
    {
        assert(callback);

        TimerId id = generateNextId();
        auto task = std::make_shared<TimerTask>(id, executeTime, std::move(callback), isRepeating, interval);
        
        m_timerQueue.push(task);
        
        spdlog::debug("[Timer] 타이머 등록: ID={}, 실행시간={}, 반복={}", 
                     id, 
                     std::chrono::duration_cast<std::chrono::milliseconds>(executeTime.time_since_epoch()).count(),
                     isRepeating);
        
        return id;
    }
}
