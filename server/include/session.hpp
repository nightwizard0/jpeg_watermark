#pragma once

#include "asio.hpp"
#include <vector>

#include "protocol.hpp"
#include "image_processor.hpp"

// Общий класс - просто на случай, если будут задачи другого типа
class Session
{
public:
    Session(ImageProcessor& proc, asio::ip::tcp::socket&& sock)
        : m_proc(proc),
        m_sock(std::move(sock)),
        m_recv_size(0), m_send_size(0),
        m_recv_buffer(nullptr), m_send_buffer(nullptr),
        m_done(false)
    {}

    Session(const Session&) = delete;
    void operator=(const Session&) = delete;

    virtual ~Session() { m_sock.close(); }

    virtual void start() = 0;
    bool is_done() const { return m_done; }

protected:
    ImageProcessor& m_proc;

    /* Интерфейс для сетевого взаимодействия для подклассов */
    void receive(uint8_t *buffer, size_t size);         // Запрос на получение порции данных размера size в буфер buffer
    void send(const uint8_t *buffer, size_t size);      // Запрос на отправку данных
    void done() { m_done = true; m_sock.close(); }

    /* Callbacks ввода/вывода, которые нужно переопределить в подклассах */
    virtual void on_receive() = 0;
    virtual void on_sent() = 0;
    virtual void on_error() = 0;

private:
    void on_receive_internal(const asio::error_code& ec, size_t bytes);
    void on_sent_internal(const asio::error_code& ec, size_t bytes);

    asio::ip::tcp::socket m_sock;
    
    size_t m_recv_size, m_send_size;
    uint8_t *m_recv_buffer;
    const uint8_t *m_send_buffer;
    bool m_done;
};

// Прием и отправка изображений по самопальному "протоколу"
class ProtoSession : public Session, public std::enable_shared_from_this<ProtoSession>
{
public:
    ProtoSession(ImageProcessor &pool, asio::ip::tcp::socket&& sock)
        : Session(pool, std::move(sock)),
        m_state(State::kReadHeader),
        m_have_response(false),
        m_have_errors(false)
    {}

    virtual void start() override { receive((uint8_t*) &m_header, sizeof(m_header)); }

    /* Получение результата наложения водяной печати */
    void complete(std::vector<uint8_t> image);

protected:
    virtual void on_receive() override;
    virtual void on_sent() override;
    virtual void on_error() override;

private:

    static const size_t m_max_image_size = 100 * 1024 * 1024; // Максимальный размер картинки - 100 Мбайт
    static const size_t m_max_text_size = 512;

    enum class State { kReadHeader, kReadText, kReadImage, kProcessing, kWriteHeader, kWriteResult };
    State m_state;

    ProtoDataHeader m_header;
    std::vector<uint8_t> m_text_buffer;
    std::vector<uint8_t> m_image_buffer;

    ProtoResponseHeader m_response;

    bool m_have_response;
    bool m_have_errors;

    void send_header(StatusCode code);
};