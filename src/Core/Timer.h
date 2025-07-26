#pragma once

#include <functional>
#include <queue>
#include <vector>
#include <chrono>
#include <memory>
#include <atomic>
#include <unordered_set>

namespace core
{
    using TimerId = uint64_t;
    using TimePoint = std::chrono::steady_clock::time_point;
    using Duration = std::chrono::milliseconds;
    using TimerCallback = std::function<void()>;

    // 타이머 작업을 나타내는 구조체
    struct TimerTask
    {
        TimerId id;
        TimePoint executeTime;
        TimerCallback callback;
        bool isRepeating;
        Duration interval;

        TimerTask(TimerId taskId, TimePoint execTime, TimerCallback cb, bool repeat = false, Duration repeatInterval = Duration::zero())
            : id(taskId)
            , executeTime(execTime)
            , callback(std::move(cb))
            , isRepeating(repeat)
            , interval(repeatInterval)
        {}

        // 우선순위 큐에서 시간이 빠른 것을 우선으로 하기 위한 비교 연산자
        bool operator>(const TimerTask& other) const
        {
            return executeTime > other.executeTime;
        }
    };

    using TimerTaskPtr = std::shared_ptr<TimerTask>;

    // 우선순위 큐 기반 타이머 클래스
    class Timer
    {
    public:
        Timer();
        ~Timer() = default;

        // 복사/이동 금지 (안전성을 위해)
        Timer(const Timer&) = delete;
        Timer& operator=(const Timer&) = delete;
        Timer(Timer&&) = delete;
        Timer& operator=(Timer&&) = delete;

        // 일회성 타이머 등록
        // delay: 지연 시간 (밀리초)
        // callback: 실행할 함수
        // 반환값: 타이머 ID (취소할 때 사용)
        TimerId scheduleOnce(Duration delay, TimerCallback callback);

        // 반복 타이머 등록
        // delay: 첫 실행까지의 지연 시간 (밀리초)
        // interval: 반복 간격 (밀리초)
        // callback: 실행할 함수
        // 반환값: 타이머 ID (취소할 때 사용)
        TimerId scheduleRepeating(Duration delay, Duration interval, TimerCallback callback);

        // 특정 시간에 실행되는 일회성 타이머 등록
        // executeTime: 실행할 시간
        // callback: 실행할 함수
        // 반환값: 타이머 ID (취소할 때 사용)
        TimerId scheduleAt(TimePoint executeTime, TimerCallback callback);

        // 타이머 취소
        // timerId: 취소할 타이머 ID
        // 반환값: 취소 성공 여부
        bool cancel(TimerId timerId);

        // 만료된 타이머들을 처리
        // 게임 루프에서 매 틱마다 호출해야 함
        // 반환값: 처리된 타이머 개수
        size_t update();

        // 현재 등록된 타이머 개수
        size_t getTimerCount() const;

        // 모든 타이머 제거
        void clear();

    private:
        // 다음 타이머 ID 생성
        TimerId generateNextId();

        // 타이머 등록 (내부 함수)
        TimerId scheduleInternal(TimePoint executeTime, TimerCallback callback, bool isRepeating = false, Duration interval = Duration::zero());

    private:
        // 우선순위 큐의 비교 함수 객체
        struct TimerTaskComparator
        {
            bool operator()(const TimerTaskPtr& lhs, const TimerTaskPtr& rhs) const
            {
                return *lhs > *rhs;
            }
        };

        // 우선순위 큐 (최소 힙 - 가장 빠른 시간이 top)
        std::priority_queue<TimerTaskPtr, std::vector<TimerTaskPtr>, TimerTaskComparator> m_timerQueue;
        
        // 취소된 타이머 ID 집합 (빠른 검색을 위해)
        std::unordered_set<TimerId> m_cancelledTimers;
        
        // 타이머 ID 생성용 카운터
        std::atomic<TimerId> m_nextTimerId;
    };
}
