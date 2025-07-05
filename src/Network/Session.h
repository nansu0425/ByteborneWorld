#pragma once

namespace net
{
    using SeesionPtr = std::shared_ptr<class Session>;
    using SessionId = int64_t;

    class Session
        : std::enable_shared_from_this<Session>
    {
    public:
        Session(SessionId sessionId);

    private:
        const SessionId m_sessionId = 0;
    };
}
