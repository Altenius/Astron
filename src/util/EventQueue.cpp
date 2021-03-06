#include "EventQueue.h"

EventQueue EventQueue::singleton;

EventQueue::~EventQueue()
{
    assert(m_task_queue.empty());
    if(m_flush_handle) {
        m_flush_handle->close();
        m_flush_handle = nullptr;
    }
}

void EventQueue::init_queue()
{
    assert(std::this_thread::get_id() == g_main_thread_id);

    m_flush_handle = g_loop->resource<uvw::AsyncHandle>();
    m_flush_handle->on<uvw::AsyncEvent>([self = this](const uvw::AsyncEvent&, uvw::AsyncHandle&) {
        self->flush_tasks();
    });
}

void EventQueue::enqueue_task(TaskCallback task)
{
    {
        std::lock_guard<std::mutex> lock(m_queue_mutex);
        m_task_queue.push(task);        
    }

    if(std::this_thread::get_id() != g_main_thread_id) {
        m_flush_handle->send();
    } else {
        flush_tasks();
    }
}

void EventQueue::flush_tasks()
{
    // We need to make absolutely certain this is running within the main (loop's) thread.
    assert(std::this_thread::get_id() == g_main_thread_id);

    if(m_in_flush) {
        // We're already in the middle of a flush_tasks operation.    
        return;
    }

    m_in_flush = true;

    {
        std::lock_guard<std::mutex> lock(m_queue_mutex);
        while(!m_task_queue.empty()) {
            TaskCallback task = m_task_queue.front();
            m_task_queue.pop();
            task();
        }
    }

    m_in_flush = false;
}
