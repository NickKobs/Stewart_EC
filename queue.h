#ifndef EXTRA_CREDIT_QUEUE_H
#define EXTRA_CREDIT_QUEUE_H

#include <cstddef>
#include <cstdio>
#include <sstream>
#include <stdexcept>
#include <string>

template <class TYPE>
class Queue {
public:
    Queue() = default;
    ~Queue() { Reset(); }

    void En_Q(const TYPE& value) {
        Node* node = new Node{value, nullptr};

        if (tail_ == nullptr) {
            head_ = node;
            tail_ = node;
        } else {
            tail_->next = node;
            tail_ = node;
        }

        ++size_;
    }

    TYPE De_Q() {
        if (isEmpty()) {
            throw std::underflow_error("cannot dequeue from an empty queue");
        }

        Node* node = head_;
        TYPE value = node->value;
        head_ = node->next;
        if (head_ == nullptr) {
            tail_ = nullptr;
        }

        delete node;
        --size_;
        return value;
    }

    TYPE Front() const {
        if (isEmpty()) {
            throw std::underflow_error("cannot read the front of an empty queue");
        }

        return head_->value;
    }

    int isEmpty() const { return size_ == 0; }
    int IsEmpty() const { return isEmpty(); }

    void Reset() {
        while (!isEmpty()) {
            De_Q();
        }
    }

    void Print() const { std::printf("\tQueue: %s\n", Get_Q_String().c_str()); }
    void Print_Q() const { Print(); }

    std::string Get_Q_String() const {
        if (isEmpty()) {
            return "[empty]";
        }

        std::ostringstream builder;
        builder << '[';

        Node* current = head_;
        while (current != nullptr) {
            builder << current->value;
            current = current->next;
            if (current != nullptr) {
                builder << ", ";
            }
        }

        builder << ']';
        return builder.str();
    }

private:
    struct Node {
        TYPE value;
        Node* next;
    };

    Node* head_ = nullptr;
    Node* tail_ = nullptr;
    std::size_t size_ = 0;
};

#endif
