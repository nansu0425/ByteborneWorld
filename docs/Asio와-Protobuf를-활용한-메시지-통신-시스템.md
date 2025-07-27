# Asio와 Protobuf를 활용한 메시지 통신 시스템

## 개요

Byteborne World 프로젝트에서는 C++ ASIO 라이브러리와 Google Protocol Buffers를 활용하여 클라이언트-서버 간 비동기 메시지 통신 시스템을 구현했습니다. 이벤트 드리븐 아키텍처를 적용하여 비동기 I/O 완료를 이벤트 형태로 전달합니다. 이 문서의 코드는 여러 클라이언트가 주기적으로 채팅을 서버로 보내면, 서버가 그 채팅과 같은 내용의 채팅을 클라이언트에게 보내는 시나리오를 기준으로 작성 됐습니다.

### 왜 이런 아키텍처를 선택했을까?

**ASIO를 선택한 이유:**
- **비동기 처리**: 하나의 스레드로 수천 개의 동시 연결을 처리할 수 있어 메모리 효율적
- **크로스 플랫폼**: Windows, Linux 모두에서 동일한 코드로 동작
- **C++ 표준**: 향후 C++ 표준에 포함될 예정인 안정적인 라이브러리

**Protocol Buffers를 선택한 이유:**
- **타입 안전성**: 컴파일 시점에 메시지 구조 검증 가능
- **효율적인 직렬화**: JSON 대비 훨씬 작은 크기와 빠른 처리 속도
- **언어 독립적**: 추후 다른 언어로 클라이언트 개발 시에도 호환 가능

**이벤트 드리븐 아키텍처의 장점:**
- **확장성**: 새로운 이벤트 타입 추가가 간단
- **디버깅 용이**: 이벤트 단위로 로그 추적 가능
- **비동기 처리**: 네트워크 I/O와 비즈니스 로직 분리

## 시스템 아키텍처

### 핵심 컴포넌트와 역할

**Session (세션)**
- 각 클라이언트와의 개별 TCP 연결을 관리하는 핵심 클래스
- 비동기 소켓 읽기/쓰기를 담당하며, 연결 상태를 추적
- Strand를 통해 스레드 안전성을 보장하면서도 성능을 유지

**Service (서비스)**
- ServerService: 클라이언트 연결 요청을 받아들이는 역할
- ClientService: 서버에 연결을 시도하는 역할
- 네트워크 계층과 애플리케이션 계층 사이의 브릿지 역할

**MessageQueue (메시지 큐)**
- 수신된 원시 바이트 데이터를 Protocol Buffers 메시지로 변환
- 메시지를 순서대로 처리할 수 있도록 큐 형태로 관리
- 메시지 타입과 세션 정보를 함께 저장

**MessageDispatcher (메시지 디스패처)**
- 메시지 타입에 따라 적절한 핸들러 함수를 호출
- 런타임에 메시지 타입을 확인하고 등록된 핸들러를 실행
- 새로운 메시지 타입 추가 시 핸들러만 등록하면 되는 확장성 제공

**MessageSerializer (메시지 시리얼라이저)**
- Protocol Buffers 메시지를 네트워크 전송이 가능한 바이트 스트림으로 변환
- 패킷 헤더(크기, 타입 정보)와 메시지 데이터를 결합
- 메모리 효율성을 위한 버퍼 관리 포함

**Buffer 시스템**
- ReceiveBuffer: 들어오는 데이터를 임시 저장하고 완전한 패킷 조립
- SendBuffer: 나가는 데이터를 효율적으로 관리하여 시스템 콜 최소화
- 동적 크기 조정과 메모리 재사용을 통한 성능 최적화

### 데이터 흐름 설명

**송신 과정:**
```
[핸들러] → [MessageSerializer] → [SendBuffer] → [Session] → [네트워크] → [클라이언트]
```

**수신 과정:**
```
[클라이언트] → [네트워크] → [Session] → [MessageQueue] → [MessageDispatcher] → [핸들러]
```

이러한 단방향 데이터 흐름으로 인해 각 컴포넌트의 책임이 명확하게 분리되어 있습니다.

## 메시지 송신 과정

메시지 송신은 애플리케이션 레벨에서 네트워크 레벨까지 여러 단계를 거쳐 이루어집니다. 각 단계는 명확한 책임을 가지고 있어 시스템의 유지보수성과 확장성을 보장합니다.

### 1. Protocol Buffers 메시지 정의

먼저 클라이언트와 서버가 주고받을 메시지의 구조를 정의합니다. Protocol Buffers의 `.proto` 파일로 정의하면 컴파일러가 자동으로 C++ 클래스를 생성해줍니다.

```proto
// Chat.proto
syntax = "proto3";
package proto;

message S2C_Chat {
    string content = 1;  // 서버에서 클라이언트로 보내는 채팅 메시지
}

message C2S_Chat {
    string content = 1;  // 클라이언트에서 서버로 보내는 채팅 메시지
}
```

**Protocol Buffers의 장점:**
- **구조화된 데이터**: 필드 추가/삭제 시 하위 호환성 유지
- **자동 코드 생성**: 직렬화/역직렬화 코드를 수동으로 작성할 필요 없음
- **타입 안전성**: 컴파일 시점에 메시지 구조 오류 발견 가능

### 2. 메시지 타입 트레이트 시스템

각 메시지 타입마다 고유한 ID를 할당하여 네트워크로 전송할 때 수신측에서 어떤 메시지인지 식별할 수 있게 합니다. 템플릿 특수화를 사용해 컴파일 타임에 타입과 ID를 연결합니다.

```cpp
// Type.h
enum class MessageType : net::PacketId {
    None = 0,
    S2C_Chat = 1000,  // 서버 → 클라이언트 메시지는 1000번대
    C2S_Chat = 2000,  // 클라이언트 → 서버 메시지는 2000번대
};

template<typename T>
struct MessageTypeTraits;

template<>
struct MessageTypeTraits<S2C_Chat> {
    static constexpr MessageType Value = MessageType::S2C_Chat;
};
```

**타입 트레이트의 역할:**
- **컴파일 타임 검증**: 잘못된 메시지 타입 사용 시 컴파일 에러 발생
- **자동 ID 매핑**: 메시지 타입에서 패킷 ID로 자동 변환
- **확장성**: 새 메시지 타입 추가 시 트레이트만 정의하면 됨

### 3. 메시지 직렬화 과정

MessageSerializer는 Protocol Buffers 메시지를 네트워크 전송이 가능한 바이트 스트림으로 변환합니다. 이 과정에서 패킷 헤더를 추가하여 수신측에서 메시지를 올바르게 파싱할 수 있게 합니다.

**MessageSerializer::serializeToSendBuffer()의 동작 과정:**

1. **메시지 크기 계산**
   ```cpp
   size_t messageSize = message.ByteSizeLong();
   size_t totalSize = sizeof(net::PacketHeader) + messageSize;
   ```
   Protocol Buffers는 메시지를 바이트 배열로 직렬화할 때의 크기를 미리 계산할 수 있습니다. 헤더 크기를 더해 전체 패킷 크기를 구합니다.

2. **SendBuffer 청크 할당**
   ```cpp
   net::SendBufferChunkPtr chunk = m_sendBufferManager.open(totalSize);
   ```
   메모리 풀에서 필요한 크기만큼의 버퍼를 할당받습니다. 청크 단위로 관리하여 메모리 단편화를 방지합니다.

3. **패킷 헤더 구성**
   ```cpp
   net::PacketHeader* header = reinterpret_cast<net::PacketHeader*>(chunk->getWritePtr());
   header->size = static_cast<net::PacketSize>(chunk->getUnwrittenSize());
   header->id = static_cast<net::PacketId>(MessageTypeTraits<TMessage>::Value);
   ```
   패킷 앞부분에 헤더를 작성합니다. 헤더에는 패킷 전체 크기와 메시지 타입 ID가 포함됩니다.

4. **Protocol Buffers 직렬화**
   ```cpp
   message.SerializeToArray(header + 1, messageSize);
   ```
   헤더 바로 뒤에 실제 메시지 데이터를 직렬화하여 저장합니다.

5. **청크 완료 처리**
   ```cpp
   chunk->onWritten(header->size);
   chunk->close();
   ```
   버퍼에 데이터 쓰기가 완료되었음을 알리고 청크를 닫습니다.

**직렬화 과정의 핵심 포인트:**
- **헤더 우선**: 수신측에서 패킷 경계를 정확히 알 수 있도록 크기 정보 포함
- **타입 정보**: 메시지 타입 ID로 수신측에서 올바른 파서 선택 가능
- **메모리 효율성**: 청크 기반 버퍼 관리로 메모리 할당 최소화

### 4. 세션을 통한 송신

직렬화된 데이터를 실제 네트워크로 전송하는 과정입니다. SessionManager가 세션을 관리하고, 각 Session이 비동기 I/O를 처리합니다.

**SessionManager::send()의 역할:**
```cpp
bool SessionManager::send(SessionId sessionId, const SendBufferChunkPtr& chunk) {
    auto session = findSession(sessionId);
    if (!session || !session->isRunning()) {
        return false;  // 세션이 없거나 종료된 경우 전송 실패
    }
    session->send(chunk);
    return true;
}
```

SessionManager는 여러 세션을 관리하는 컨테이너 역할을 합니다. 특정 세션 ID로 메시지를 보내거나, 모든 세션에 브로드캐스트할 수 있습니다.

**Session::send()의 비동기 처리:**

1. **Strand를 통한 스레드 안전 처리**
   ```cpp
   asio::post(m_strand, [this, self = shared_from_this(), chunk = chunk]() {
       bool writeInProgress = !m_sendQueue.empty();
       m_sendQueue.push_back(chunk);
       
       if (!writeInProgress) {
           asyncWrite();
       }
   });
   ```
   
   **Strand의 중요성:**
   - **순서 보장**: 같은 세션의 메시지들이 순서대로 처리됨
   - **스레드 안전**: 여러 스레드에서 동시에 send 호출해도 안전
   - **데드락 방지**: 락 없이도 동시성 문제 해결

2. **비동기 쓰기 작업**
   ```cpp
   asio::async_write(
       m_socket,
       asio::buffer(
           m_sendQueue.front()->getReadPtr(),
           m_sendQueue.front()->getWrittenSize()),
       asio::bind_executor(m_strand, 
           [this, self = shared_from_this()](const asio::error_code& error, size_t bytesWritten) {
               onWritten(error, bytesWritten);
           }));
   ```
   
   **비동기 쓰기의 장점:**
   - **논블로킹**: 네트워크 I/O가 완료될 때까지 대기하지 않음
   - **효율성**: 운영체제의 소켓 버퍼를 최대한 활용
   - **에러 처리**: 네트워크 오류 발생 시 콜백으로 처리

## 메시지 수신 과정

메시지 수신은 송신의 역순으로 이루어지며, 네트워크에서 들어온 원시 바이트 데이터를 최종적으로 의미 있는 비즈니스 로직까지 전달하는 과정입니다.

### 1. 소켓 데이터 수신

Session은 클라이언트와의 TCP 연결을 관리하며, 지속적으로 들어오는 데이터를 비동기적으로 수신합니다.

**Session::asyncRead()의 동작:**
```cpp
m_socket.async_read_some(
    asio::buffer(
        m_receiveBuffer.getWritePtr(),
        m_receiveBuffer.getUnwrittenSize()),
    asio::bind_executor(m_strand,
        [this, self = shared_from_this()](const asio::error_code& error, size_t bytesRead) {
            onRead(error, bytesRead);
        }));
```

**비동기 읽기의 특징:**
- **논블로킹**: 데이터가 올 때까지 기다리지 않고 다른 작업 수행 가능
- **버퍼 관리**: ReceiveBuffer가 들어오는 데이터를 임시 저장
- **부분 수신 대응**: TCP의 특성상 메시지가 여러 번에 나뉘어 올 수 있음

**ReceiveBuffer의 역할:**
- **데이터 안정성**: 읽기/쓰기 위치를 추적하여 데이터 덮어쓰기 방지
- **오프셋 이동**: 더 이상 읽을 데이터가 없을 경우에만 읽기/쓰기 위치를 맨 앞으로 이동
- **데이터 이동**: 쓰기를 진행할 공간이 부족할 경우 데이터를 앞으로 이동

### 2. 수신 이벤트 생성

데이터 수신이 완료되면 즉시 처리하지 않고 이벤트로 변환하여 메인 스레드에서 처리합니다. 이는 네트워크 I/O와 비즈니스 로직을 분리하는 핵심 메커니즘입니다.

**Session::onRead()의 이벤트 생성:**
```cpp
void Session::onRead(const asio::error_code& error, size_t bytesRead) {
    if (error) {
        handleError(error);  // 네트워크 오류 처리
        return;
    }
    
    m_receiveBuffer.onWritten(bytesRead);  // 버퍼에 데이터 기록
    
    // 이벤트 큐에 수신 이벤트 추가
    SessionEventPtr event = std::make_shared<SessionReceiveEvent>(m_sessionId);
    m_eventQueue.push(std::move(event));
}
```

**이벤트 드리븐 방식의 장점:**
- **비동기 처리**: I/O 스레드와 로직 처리 스레드 분리
- **순서 보장**: 이벤트 큐를 통해 메시지 처리 순서 보장
- **에러 격리**: 하나의 메시지 처리 오류가 전체 시스템에 영향 주지 않음

### 3. 패킷 파싱 및 메시지 큐 추가

수신 이벤트가 메인 스레드에서 처리될 때, 원시 바이트 데이터를 Protocol Buffers 메시지로 변환합니다.

**MessageQueue::push()의 파싱 과정:**
```cpp
void MessageQueue::push(net::SessionId sessionId, const net::PacketView& packetView) {
    assert(packetView.isValid());
    
    // 메시지 큐 엔트리 생성
    MessageQueueEntry entry;
    entry.sessionId = sessionId;
    entry.messageType = static_cast<MessageType>(packetView.header->id);
    entry.message = MessageFactory::createMessage(entry.messageType);
    
    // 패킷 페이로드를 메시지로 파싱
    const void* payload = packetView.payload;
    const int payloadSize = packetView.header->size - sizeof(net::PacketHeader);
    const bool parsingResult = entry.message->ParseFromArray(payload, payloadSize);
    assert(parsingResult);
    
    // 파싱 성공시 큐에 추가
    m_queue.emplace_back(std::move(entry));
}
```

**패킷 파싱의 핵심 단계:**

1. **헤더 검증**: 패킷 크기와 타입 ID 확인
2. **타입 매핑**: 패킷 ID로부터 적절한 Protocol Buffers 메시지 타입 결정
3. **팩토리 생성**: MessageFactory를 통해 메시지 객체 인스턴스 생성
4. **역직렬화**: Protocol Buffers의 ParseFromArray로 바이트 → 객체 변환
5. **큐 저장**: 파싱된 메시지를 처리 대기 큐에 추가

**PacketView의 역할:**
- **안전한 접근**: 패킷 데이터에 대한 안전한 포인터 래퍼
- **헤더/페이로드 분리**: 패킷 구조를 명확히 구분
- **유효성 검사**: 패킷이 올바른 형식인지 확인

### 4. 메시지 디스패칭

파싱된 메시지를 타입에 따라 적절한 핸들러 함수로 전달하는 과정입니다.

**MessageDispatcher::dispatch()의 동작:**
```cpp
void MessageDispatcher::dispatch(const MessageQueueEntry& entry) {
    auto it = m_handlers.find(entry.messageType);
    assert(it != m_handlers.end());
    
    if (it != m_handlers.end()) {
        it->second(entry.sessionId, entry.message);  // 등록된 핸들러 호출
    }
}
```

**디스패처의 설계 원칙:**
- **타입 기반 라우팅**: 메시지 타입에 따라 자동으로 핸들러 선택
- **런타임 바인딩**: 실행 시점에 핸들러 등록/해제 가능
- **확장성**: 새로운 메시지 타입 추가 시 핸들러만 등록하면 됨

### 5. 핸들러 실행

최종적으로 비즈니스 로직이 실행되는 단계입니다. 각 애플리케이션(WorldServer, DummyClient)에서 메시지별로 적절한 처리를 수행합니다.

**WorldServer 메시지 핸들러 등록:**
```cpp
void WorldServer::registerMessageHandlers() {
    m_messageDispatcher.registerHandler(
        proto::MessageType::C2S_Chat,
        [this](net::SessionId sessionId, const proto::MessagePtr& message) {
            handleMessage(sessionId, *std::static_pointer_cast<proto::C2S_Chat>(message));
        });
}

void WorldServer::handleMessage(net::SessionId sessionId, const proto::C2S_Chat& message) {
    // 받은 채팅 메시지를 그대로 응답으로 전송 (에코 서버 동작)
    proto::S2C_Chat response;
    response.set_content(message.content());
    
    net::SendBufferChunkPtr chunk = m_messageSerializer.serializeToSendBuffer(response);
    bool result = m_sessionManager.send(sessionId, chunk);
}
```

**핸들러 등록의 특징:**
- **람다 함수**: 간결하고 지역적인 처리 로직 구현
- **타입 캐스팅**: 제네릭 MessagePtr을 구체적인 메시지 타입으로 변환
- **세션 정보**: 메시지와 함께 송신자 정보도 전달받아 응답 가능

**비즈니스 로직 처리:**
- **상태 변경**: 게임 상태, 플레이어 정보 등 업데이트
- **응답 생성**: 클라이언트로 보낼 응답 메시지 생성
- **브로드캐스트**: 필요시 다른 클라이언트들에게도 정보 전파

## 세션 관리

세션(Session)은 개별 클라이언트와의 TCP 연결을 나타내는 핵심 개념입니다. 각 세션은 독립적으로 생명주기를 가지며, 연결 설정부터 종료까지 모든 과정을 관리합니다.

### 세션 생명주기

세션의 생명주기는 크게 생성, 시작, 실행, 종료 4단계로 나뉩니다.

1. **세션 생성**
   ```cpp
   SessionPtr Session::createInstance(asio::ip::tcp::socket&& socket, SessionEventQueue& eventQueue) {
       static SessionId nextSessionId = 1;  // 고유 ID 생성
       auto session = std::make_shared<Session>(nextSessionId++, std::move(socket), eventQueue);
       return session;
   }
   ```
   
   **생성 과정의 특징:**
   - **고유 ID 할당**: 각 세션마다 유니크한 식별자 부여
   - **소켓 이동**: TCP 소켓의 소유권을 세션으로 이전
   - **이벤트 큐 연결**: 세션에서 발생하는 이벤트를 메인 스레드로 전달할 채널 설정
   - **shared_ptr 사용**: 비동기 콜백에서 안전한 참조 관리

2. **세션 시작**
   ```cpp
   void Session::start() {
       m_running.store(true);  // 원자적 플래그 설정
       receive(); // 비동기 읽기 시작
   }
   ```
   
   **시작 단계의 역할:**
   - **상태 변경**: 세션을 활성 상태로 전환
   - **비동기 읽기 시작**: 클라이언트로부터 오는 데이터 수신 대기
   - **스레드 안전**: atomic 변수로 동시 접근 시 안전성 보장

3. **세션 종료**
   ```cpp
   void Session::stop() {
       if (m_running.exchange(false)) {  // 원자적 상태 변경 및 이전 값 확인
           asio::post(m_strand, [this, self = shared_from_this()]() {
               close();  // 소켓 정리 및 리소스 해제
           });
       }
   }
   ```
   
   **종료 과정의 안전성:**
   - **중복 종료 방지**: exchange를 사용해 한 번만 종료 처리
   - **Strand 사용**: 종료 작업도 세션의 Strand에서 실행하여 동시성 문제 방지
   - **참조 유지**: shared_from_this()로 종료 과정 중 객체 소멸 방지

### 세션 이벤트 처리

세션에서 발생하는 이벤트들은 메인 스레드에서 일관되게 처리됩니다. 이는 멀티스레드 환경에서도 안정적인 동작을 보장합니다.

**WorldServer에서의 세션 이벤트 처리:**
```cpp
void WorldServer::processSessionEvents() {
    net::SessionEventPtr event;
    while (m_sessionEventQueue.pop(event)) {  // 큐에서 이벤트 하나씩 처리
        switch (event->type) {
        case net::SessionEventType::Close:
            handleSessionEvent(*static_cast<net::SessionCloseEvent*>(event.get()));
            break;
        case net::SessionEventType::Receive:
            handleSessionEvent(*static_cast<net::SessionReceiveEvent*>(event.get()));
            break;
        }
    }
}
```

**이벤트 기반 처리의 장점:**
- **단일 스레드 처리**: 모든 세션 관련 로직이 메인 스레드에서 실행되어 동기화 불필요
- **순서 보장**: 이벤트 큐를 통해 세션별 이벤트 처리 순서 보장
- **확장성**: 새로운 이벤트 타입 추가가 용이

**세션 종료 이벤트 처리:**
```cpp
void WorldServer::handleSessionEvent(net::SessionCloseEvent& event) {
    m_sessionManager.removeSession(event.sessionId);
    spdlog::info("[WorldServer] 세션 {} 연결 종료", event.sessionId);
}
```

**수신 이벤트 처리:**
```cpp
void WorldServer::handleSessionEvent(net::SessionReceiveEvent& event) {
    auto session = m_sessionManager.findSession(event.sessionId);
    if (!session) return;
    
    // 수신 버퍼에서 완성된 패킷들을 메시지 큐로 이동
    net::PacketView packetView;
    while (session->getFrontPacket(packetView)) {
        m_messageQueue.push(event.sessionId, packetView);
        session->popFrontPacket();
    }
}
```

### SessionManager의 역할

SessionManager는 여러 세션을 통합 관리하는 컨테이너입니다.

**주요 기능:**
- **세션 등록/해제**: 새로운 연결 추가 및 종료된 연결 제거
- **메시지 전송**: 특정 세션 또는 모든 세션에 메시지 전송
- **세션 검색**: 세션 ID로 특정 세션 찾기
- **생명주기 관리**: 모든 세션의 일괄 종료

**브로드캐스트 기능:**
```cpp
void SessionManager::broadcast(const SendBufferChunkPtr& chunk) {
    for (const auto& pair : m_sessions) {
        pair.second->send(chunk);  // 모든 활성 세션에 메시지 전송
    }
}
```

이는 채팅 시스템이나 게임 상태 동기화에 필수적인 기능입니다.

## 서비스 관리

Service는 네트워크 연결의 생성과 관리를 담당하는 상위 레벨 컴포넌트입니다. ServerService와 ClientService로 나뉘어 각각 서버와 클라이언트의 역할을 수행합니다.

### ServerService 구조

ServerService는 클라이언트의 연결 요청을 받아들이는 역할을 합니다. TCP Acceptor를 사용하여 지정된 포트에서 연결을 대기합니다.

1. **서버 시작 및 Accept**
   ```cpp
   void ServerService::start() {
       m_running.store(true);
       asyncAccept();  // 연결 수락 시작
   }
   
   void ServerService::asyncAccept() {
       m_acceptor.async_accept(
           asio::bind_executor(m_strand,
               [this, self = shared_from_this()](const asio::error_code& error, asio::ip::tcp::socket socket) {
                   onAccepted(error, std::move(socket));
               }));
   }
   ```
   
   **Accept 과정의 특징:**
   - **비동기 처리**: 연결 요청을 기다리는 동안 다른 작업 수행 가능
   - **Strand 사용**: 멀티스레드 환경에서 Accept 콜백의 순서 보장
   - **재귀 호출**: 한 연결을 받은 후 즉시 다음 연결 대기

2. **Accept 이벤트 생성**
   ```cpp
   void ServerService::onAccepted(const asio::error_code& error, asio::ip::tcp::socket&& socket) {
       if (!error) {
           const auto& remoteEndpoint = socket.remote_endpoint();
           spdlog::info("[ServerService] 클라이언트 연결: {}:{}", 
                       remoteEndpoint.address().to_string(), remoteEndpoint.port());
           
           // 이벤트 큐에 accept 이벤트 추가
           ServiceEventPtr event = std::make_shared<ServiceAcceptEvent>(std::move(socket));
           m_eventQueue.push(std::move(event));
       } else {
           handleError(error);
       }
       
       asyncAccept(); // 다음 연결 대기
   }
   ```
   
   **Accept 이벤트의 역할:**
   - **소켓 전달**: 새로 생성된 TCP 소켓을 애플리케이션 계층으로 전달
   - **연결 정보 로깅**: 클라이언트의 IP 주소와 포트 정보 기록
   - **연속 처리**: Accept 완료 후 즉시 다음 연결 대기 상태로 전환

### ClientService 구조

ClientService는 서버에 연결을 시도하는 클라이언트 측 서비스입니다. DNS 해석부터 실제 연결까지 전체 과정을 관리합니다.

1. **연결 시도**
   ```cpp
   void ClientService::start() {
       m_running.store(true);
       asyncResolve();  // 먼저 서버 주소 해석
   }
   
   void ClientService::asyncConnect(size_t socketIndex) {
       m_sockets[socketIndex].async_connect(
           *m_resolveResults.begin(),
           asio::bind_executor(m_strand,
               [this, self = shared_from_this(), socketIndex](const asio::error_code& error) {
                   onConnected(error, socketIndex);
               }));
   }
   ```
   
   **클라이언트 연결의 단계:**
   1. **DNS 해석**: 호스트명을 IP 주소로 변환
   3. **비동기 연결**: index 기반으로 각 소켓이 서버에 TCP 연결 시도
   4. **결과 처리**: 성공/실패에 따른 적절한 처리

2. **Connect 이벤트 생성**
   ```cpp
   void ClientService::onConnected(const asio::error_code& error, size_t socketIndex) {
       if (!error) {
           spdlog::info("[ClientService] 서버 연결 성공: Socket {}", socketIndex);
           
           ServiceEventPtr event = std::make_shared<ServiceConnectEvent>(std::move(m_sockets[socketIndex]));
           m_eventQueue.push(std::move(event));
       } else {
           spdlog::error("[ClientService] 서버 연결 실패: {}", error.message());
           handleError(error);
       }
   }
   ```
   
   **Connect 이벤트의 특징:**
   - **성공/실패 처리**: 연결 결과에 따른 분기 처리
   - **소켓 이동**: 연결된 소켓을 애플리케이션 계층으로 전달
   - **에러 처리**: 연결 실패 시 적절한 에러 처리 및 로깅

### 서비스 이벤트 처리

서비스에서 생성된 이벤트들은 애플리케이션의 메인 루프에서 처리됩니다.

**WorldServer에서의 서비스 이벤트 처리:**
```cpp
void WorldServer::processServiceEvents() {
    net::ServiceEventPtr event;
    while (m_serviceEventQueue.pop(event)) {
        switch (event->type) {
        case net::ServiceEventType::Accept:
            handleServiceEvent(*static_cast<net::ServiceAcceptEvent*>(event.get()));
            break;
        case net::ServiceEventType::Close:
            handleServiceEvent(*static_cast<net::ServiceCloseEvent*>(event.get()));
            break;
        }
    }
}

void WorldServer::handleServiceEvent(net::ServiceAcceptEvent& event) {
    // 새로운 클라이언트 연결을 세션으로 변환
    auto session = net::Session::createInstance(std::move(event.socket), m_sessionEventQueue);
    m_sessionManager.addSession(session);
    session->start();
}
```

**DummyClient에서의 서비스 이벤트 처리:**
```cpp
void DummyClient::handleServiceEvent(net::ServiceConnectEvent& event) {
    // 서버 연결을 세션으로 변환
    auto session = net::Session::createInstance(std::move(event.socket), m_sessionEventQueue);
    m_sessionManager.addSession(session);
    session->start();
}
```

### 서비스 설계 원칙

**추상화와 확장성:**
- **공통 인터페이스**: Service 기본 클래스로 공통 기능 제공
- **특화된 구현**: ServerService와 ClientService로 역할별 특화
- **이벤트 기반**: 네트워크 계층과 애플리케이션 계층 분리

**에러 처리와 안정성:**
- **포괄적 에러 처리**: 다양한 네트워크 에러 상황 대응
- **로깅**: 상세한 연결 상태 로깅으로 디버깅 지원

## 이벤트 전파 및 처리 메커니즘

이벤트 드리븐 아키텍처는 이 시스템의 핵심 설계 원칙입니다. 네트워크 I/O 스레드에서 발생하는 모든 중요한 사건들을 이벤트로 변환하여 메인 스레드에서 순차적으로 처리합니다.

### 이벤트 시스템의 철학

**왜 이벤트 드리븐인가?**
- **동시성 문제 해결**: 멀티스레드 환경에서 최소한의 락(여러 스레드에서 접근하는 자료구조)으로 안전한 데이터 접근
- **순서 보장**: 네트워크 이벤트의 처리 순서 보장
- **확장성**: 새로운 이벤트 타입 추가가 간단
- **디버깅 용이**: 이벤트 단위로 시스템 상태 추적 가능

### 이벤트 타입 정의

시스템에서 사용하는 이벤트는 크게 세션 이벤트와 서비스 이벤트로 구분됩니다.

```cpp
// 세션 이벤트: 개별 클라이언트 연결에서 발생하는 이벤트
enum class SessionEventType {
    None,      // 초기값
    Close,     // 세션 연결 종료
    Receive,   // 데이터 수신 완료
};

// 서비스 이벤트: 네트워크 서비스 계층에서 발생하는 이벤트  
enum class ServiceEventType {
    None,      // 초기값
    Close,     // 서비스 종료
    Accept,    // 새로운 클라이언트 연결 수락 (서버 전용)
    Connect,   // 서버 연결 성공 (클라이언트 전용)
};
```

**이벤트 구조체의 설계:**
```cpp
struct SessionEvent {
    SessionEventType type;
    SessionId sessionId;  // 어떤 세션에서 발생한 이벤트인지 식별
};

struct ServiceEvent {
    ServiceEventType type;
    // 서비스 전체에 영향을 주는 이벤트이므로 추가 식별자 불필요
};

struct ServiceAcceptEvent : public ServiceEvent {
    asio::ip::tcp::socket socket;  // 새로 연결된 소켓을 포함
};
```

### 이벤트 전파 흐름

이벤트는 다음과 같은 경로로 전파됩니다:

1. **네트워크 계층에서 이벤트 발생**
   - 소켓에서 데이터 수신 완료
   - 클라이언트 연결 수락
   - 네트워크 오류 발생

2. **이벤트 객체 생성**
   - 발생한 상황에 맞는 이벤트 객체 생성
   - 필요한 컨텍스트 정보 (세션 ID, 소켓 등) 포함

3. **이벤트 큐에 추가**
   - 스레드 안전한 큐에 이벤트 저장
   - I/O 스레드에서 메인 스레드로 데이터 전달

4. **메인 루프에서 처리**
   - 큐에서 이벤트를 하나씩 꺼내서 처리
   - 이벤트 타입에 따라 적절한 핸들러 호출

5. **비즈니스 로직 실행**
   - 이벤트에 따른 게임 상태 변경
   - 필요시 다른 클라이언트에게 정보 전파

### 메인 루프 구조

**WorldServer의 메인 루프:**
```cpp
void WorldServer::loop() {
    constexpr auto TickInterval = std::chrono::milliseconds(50);  // 20 FPS
    
    while (m_running.load()) {
        auto start = std::chrono::steady_clock::now();
        
        processServiceEvents();    // 1. 서비스 이벤트 처리
        processSessionEvents();    // 2. 세션 이벤트 처리  
        processMessages();         // 3. 메시지 처리
        m_timer.update();         // 4. 타이머 업데이트
        
        // 5. 다음 틱까지 대기 (정확한 주기 유지)
        waitForNextTick(start, TickInterval);
    }
}
```

**메인 루프의 설계 원칙:**
- **고정 틱률**: 50ms 간격으로 일정한 처리 주기 유지
- **순차 처리**: 이벤트 → 메시지 → 타이머 순서로 처리
- **원자적 연산**: atomic 변수로 스레드 안전한 종료 조건 확인

**처리 단계별 설명:**

1. **서비스 이벤트 처리**: 새로운 연결이나 서비스 종료 등 전역적 변화 처리
2. **세션 이벤트 처리**: 개별 클라이언트의 연결 상태 변화 처리
3. **메시지 처리**: 실제 게임 로직이나 비즈니스 로직 실행
4. **타이머 업데이트**: 지연된 작업이나 주기적 작업 실행

### DummyClient 구조와 특징

DummyClient는 서버를 테스트하기 위한 시뮬레이션 클라이언트입니다.

```cpp
void DummyClient::loop() {
    constexpr auto TickInterval = std::chrono::milliseconds(50);  // 20 FPS
    
    while (m_running.load()) {
        auto start = std::chrono::steady_clock::now();
        
        processServiceEvents();    // 서버 연결 상태 확인
        processSessionEvents();    // 서버로부터의 응답 처리
        processMessages();         // 받은 메시지 처리
        m_timer.update();         // 주기적 메시지 전송
        
        waitForNextTick(start, TickInterval);
    }
}
```

**DummyClient의 특별한 기능 - 자동 메시지 전송:**
```cpp
// 서버 연결 성공 시 주기적 채팅 메시지 전송 시작
void DummyClient::handleServiceEvent(net::ServiceConnectEvent& event) {
    auto session = net::Session::createInstance(std::move(event.socket), m_sessionEventQueue);
    m_sessionManager.addSession(session);
    session->start();
    
    SessionId sessionId = session->getSessionId();
    
    // 250ms 간격으로 채팅 메시지 전송하는 타이머 등록
    m_timer.scheduleRepeating(
        std::chrono::milliseconds(0),      // 즉시 시작
        std::chrono::milliseconds(250),    // 250ms 간격
        [this, sessionId]() -> bool {      // 반복 실행할 작업
            proto::C2S_Chat chat;
            chat.set_content("Hello, Byteborne World!");
            
            auto chunk = m_messageSerializer.serializeToSendBuffer(chat);
            return m_sessionManager.send(sessionId, chunk);
        });
}
```

**자동 메시지 전송의 목적:**
- **부하 테스트**: 서버의 메시지 처리 성능 확인
- **연결 유지**: 주기적 통신으로 연결 상태 확인
- **시스템 검증**: 송신-수신-응답 전체 과정의 정상 동작 확인

### 이벤트 처리의 장점

**성능 측면:**
- **캐시 효율성**: 메인 스레드에서 순차 처리로 CPU 캐시 활용 극대화
- **컨텍스트 스위칭 최소화**: I/O 스레드 풀은 소수의 스레드로 구성
- **배치 처리**: 여러 이벤트를 한 번에 처리하여 오버헤드 감소

**개발 측면:**
- **디버깅 용이**: 이벤트 로그로 시스템 상태 추적 가능
- **테스트 용이**: 이벤트 단위로 단위 테스트 작성 가능
- **확장성**: 새로운 이벤트 타입 추가 시 기존 코드 수정 최소화

## 버퍼 관리 시스템

효율적인 네트워크 통신을 위해서는 메모리 관리가 핵심입니다. 이 시스템에서는 수신과 송신을 위한 별도의 버퍼 시스템을 구현하여 성능을 최적화했습니다.

### ReceiveBuffer - 수신 버퍼 시스템

ReceiveBuffer는 네트워크에서 들어오는 데이터를 임시 저장하고, 완전한 패킷이 수신될 때까지 대기하는 역할을 합니다.

**핵심 설계 원칙:**
- **데이터 안정성**: 읽기/쓰기 위치를 추적하여 데이터 덮어쓰기 방지
- **오프셋 이동**: 더 이상 읽을 데이터가 없을 경우에만 읽기/쓰기 위치를 맨 앞으로 이동
- **데이터 이동**: 쓰기를 진행할 공간이 부족할 경우 데이터를 앞으로 이동

**주요 기능들:**

1. **오프셋 기반 관리**
   ```cpp
   class ReceiveBuffer {
   private:
       std::vector<uint8_t> m_buffer;
       size_t m_readOffset = 0;   // 다음에 읽을 위치
       size_t m_writeOffset = 0;  // 다음에 쓸 위치
   };
   ```

2. **동적 오프셋 리셋**
   ```cpp
   void ReceiveBuffer::resetOffsets() {
       if (m_readOffset == m_writeOffset) {
           // 모든 데이터를 읽었으면 오프셋 초기화
           m_readOffset = 0;
           m_writeOffset = 0;
       } else if (m_readOffset > 0 && getUnwrittenSize() < m_size) {
           // 읽은 데이터 제거하고 남은 데이터를 앞으로 이동
           std::memmove(m_buffer.data(), getReadPtr(), getUnreadSize());
           m_writeOffset -= m_readOffset;
           m_readOffset = 0;
       }
   }
   ```

**오프셋 리셋의 중요성:**
- **메모리 절약**: 모든 데이터를 읽은 경우 버퍼의 모든 영역을 재활용 가능
- **성능 최적화**: 버퍼 전체 재할당대신 쓰기 공간이 부족할 경우에만 데이터 이동하는 방식으로 비용 절약

### SendBuffer - 송신 버퍼 시스템

SendBuffer는 청크(Chunk) 기반으로 설계되어 효율적인 메모리 풀링과 동시 접근을 지원합니다.

**청크 기반 설계의 장점:**
- **병렬 처리**: 여러 스레드가 서로 다른 청크에 동시 접근 가능
- **메모리 풀링**: 청크 재사용으로 빈번한 할당/해제 비용 절약
- **생명주기 관리**: 각 청크가 독립적인 생명주기를 가져 안전한 비동기 처리

**SendBufferChunk의 구조:**
```cpp
class SendBufferChunk {
private:
    SendBufferPtr m_owner;      // 부모 버퍼 참조
    uint8_t* m_chunk;          // 실제 데이터 포인터
    size_t m_openSize;         // 할당받은 크기
    size_t m_writeOffset;      // 현재 쓰기 위치
    bool m_closed;             // 쓰기 완료 여부
};
```

**SendBufferManager의 역할:**
```cpp
class SendBufferManager {
public:
    SendBufferChunkPtr open(size_t size) {
        // 현재 버퍼에 공간이 충분하면 청크 반환
        if (m_currentBuffer->getFreeSize() >= size) {
            return m_currentBuffer->open(size);
        }
        
        // 공간이 부족하면 새 버퍼 생성
        m_currentBuffer = SendBuffer::create();
        return m_currentBuffer->open(size);
    }
    
private:
    SendBufferPtr m_currentBuffer;
};
```

**청크 사용 과정:**
1. **청크 열기**: 필요한 크기만큼 청크 할당
2. **데이터 쓰기**: 직렬화된 메시지 데이터를 청크에 저장
3. **청크 닫기**: 쓰기 완료 후 읽기 전용 상태로 전환
4. **네트워크 전송**: 청크 데이터를 소켓으로 전송
5. **자동 해제**: 전송 완료 후 청크 자동 해제

### 버퍼 시스템의 성능 최적화

**메모리 접근 패턴 최적화:**
- **순차 접근**: 대부분의 읽기/쓰기가 순차적으로 이루어져 캐시 친화적
- **미리 할당**: 버퍼 크기를 예측하여 재할당 횟수 최소화
- **정렬된 접근**: 메모리 정렬을 고려한 데이터 배치

**동시성 최적화:**
- **락 프리**: SendBufferChunk는 open -> close는 메인 스레드가 처리(싱글 스레드)
- **스레드 로컬**: 각 세션별로 독립적인 ReceiveBuffer 사용
- **RAII**: 스마트 포인터를 통해서 thread-safe하게 SendBuffer 자동 해제

## 성능 최적화 요소

이 시스템은 높은 동시성과 낮은 지연시간을 달성하기 위해 여러 가지 성능 최적화 기법을 적용했습니다.

### 1. 비동기 I/O를 통한 확장성

**전통적인 동기 I/O의 문제점:**
- 각 클라이언트마다 별도 스레드 필요 → 메모리 사용량 급증
- 컨텍스트 스위칭 오버헤드 → CPU 효율성 저하
- 스레드 수 제한 → 동시 접속자 수 제한

**ASIO 비동기 I/O의 장점:**
```cpp
// 하나의 I/O 스레드로 수천 개의 세션 처리 가능
void Session::asyncRead() {
    m_socket.async_read_some(
        asio::buffer(m_receiveBuffer.getWritePtr(), m_receiveBuffer.getUnwrittenSize()),
        [this, self = shared_from_this()](const asio::error_code& error, size_t bytesRead) {
            // 콜백에서 결과 처리 - 블로킹 없음
            onRead(error, bytesRead);
        });
    // 이 함수는 즉시 반환됨
}
```

### 2. Strand를 통한 효율적 동시성 제어

**전통적인 락(Lock)의 문제점:**
- 락 경합으로 인한 성능 저하
- 데드락 위험성
- 복잡한 락 순서 관리 필요

**Strand 기반 동시성의 장점:**
```cpp
class Session {
private:
    asio::strand<asio::ip::tcp::socket::executor_type> m_strand;
    
    void send(const SendBufferChunkPtr& chunk) {
        // 모든 세션 작업을 동일한 Strand에서 순차 실행
        asio::post(m_strand, [this, chunk = chunk]() {
            m_sendQueue.push_back(chunk);
            if (m_sendQueue.size() == 1) {
                asyncWrite();  // 락 없이도 안전한 큐 관리
            }
        });
    }
};
```

**Strand의 성능 이점:**
- **락 프리**: 명시적 락 없이도 데이터 안전성 보장
- **순서 보장**: 같은 세션의 작업들이 순서대로 처리
- **캐시 친화적**: 연관된 데이터 접근이 같은 스레드에서 이루어짐

### 3. 메모리 풀링을 통한 할당 최적화

**빈번한 메모리 할당의 문제점:**
- 힙 단편화로 인한 성능 저하
- 메모리 할당/해제 시스템 콜 오버헤드
- 가비지 컬렉션 압박 (C++에서는 해당 없지만 원리 동일)

**SendBuffer 풀링 전략:**
```cpp
class SendBufferManager {
    SendBufferChunkPtr open(size_t size) {
        // 현재 버퍼에 공간이 있으면 재사용
        if (m_currentBuffer && m_currentBuffer->getFreeSize() >= size) {
            return m_currentBuffer->open(size);  // 새 할당 없음
        }
        
        // 공간 부족 시에만 새 버퍼 생성
        m_currentBuffer = SendBuffer::create(BUFFER_SIZE);
        return m_currentBuffer->open(size);
    }
    
private:
    static constexpr size_t BUFFER_SIZE = 4096;  // 페이지 크기와 정렬
    SendBufferPtr m_currentBuffer;
};
```

**풀링의 성능 효과:**
- **할당 횟수 감소**: 4KB 버퍼 하나로 수십 개의 작은 메시지 처리
- **메모리 지역성**: 연관된 데이터가 같은 페이지에 위치
- **시스템 콜 감소**: malloc/free 호출 횟수 대폭 감소

### 4. 템플릿 기반 컴파일 타임 최적화

**런타임 타입 체크의 오버헤드:**
- 메시지 타입 확인을 위한 조건문
- 가상 함수 호출 오버헤드
- 타입 캐스팅 비용

**템플릿 특수화를 통한 최적화:**
```cpp
template<typename TMessage>
net::SendBufferChunkPtr MessageSerializer::serializeToSendBuffer(const TMessage& message) {
    // 컴파일 타임에 타입 확정 - 런타임 체크 불필요
    static_assert(std::is_base_of_v<Message, TMessage>);
    
    // 컴파일 타임에 메시지 ID 결정
    constexpr auto messageType = MessageTypeTraits<TMessage>::Value;
    
    // 컴파일러가 특정 타입에 최적화된 코드 생성
    size_t messageSize = message.ByteSizeLong();
    // ...
}
```

**컴파일 타임 최적화 효과:**
- **분기 제거**: 런타임 타입 체크 코드 완전 제거
- **인라인 최적화**: 컴파일러가 더 적극적으로 인라인 최적화 수행
- **코드 특화**: 각 메시지 타입에 최적화된 전용 코드 생성
