/**
 * @file Singleton.h
 * @brief CRTP singleton base — thread-safe via C++11 magic statics
 */
#ifndef SINGLETON_H
#define SINGLETON_H

template <typename Derived>
class Singleton {
public:
    static Derived& instance() {
        static Derived instance;
        return instance;
    }

    Singleton(const Singleton&) = delete;
    Singleton& operator=(const Singleton&) = delete;

protected:
    Singleton()  = default;
    ~Singleton() = default;
};

#endif
