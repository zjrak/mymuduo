#pragma once

/* 
    继承的基类不可复制和赋值
    可以进行正常的析构和构造
 */

class nocopyable
{
public:
    nocopyable(const nocopyable&) = delete;
    void operator=(const nocopyable&) = delete;
protected:
    nocopyable() = default;
    ~nocopyable() = default;
};