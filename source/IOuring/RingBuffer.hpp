//
// Created by _edd.ie_ on 12/02/2026.
//

#ifndef RINGBUFFER_H
#define RINGBUFFER_H

#include <memory>
#include <stdexcept>

template <typename T>
class RingBuffer
{
    const std::unique_ptr<T[]> buffer;
    size_t capacity;
    size_t size = 0;
    size_t head = 0;
    size_t tail = 0;

public:
    RingBuffer(size_t maxSize) : buffer(std::make_unique<T[]>(maxSize)), capacity(maxSize) {}

    bool isFull()
    {
        return size == capacity;
    }

    bool isEmpty()
    {
        return size == 0;
    }

    void put(T item)
    {
        buffer[head] = item;

        if (isFull())
        {
            // If full, the tail is "pushed" forward by the new head
            tail = (tail + 1) % capacity;
        }
        else
        {
            // If not full, the total count increases
            size++;
        }

        // ALWAYS move the head forward after writing
        head = (head + 1) % capacity;
    }

    T get()
    {
        if (size == 0)
            throw std::runtime_error("Buffer empty");

        T item = buffer[tail];
        tail = (tail + 1) % capacity;
        size--;
        return item;
    }

    T peek() const
    {
        if (size == 0)
            throw std::runtime_error("Buffer empty");

        return buffer[tail];
    }
};

#endif // RINGBUFFER_H