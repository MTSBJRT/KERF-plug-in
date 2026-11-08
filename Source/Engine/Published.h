#pragma once
#include <juce_core/juce_core.h>
#include <array>
#include <memory>

namespace kerf
{
/** Hands an immutable object from the message thread to the audio thread.
    The audio thread never blocks: if the lock is busy it keeps using the
    previous snapshot. The last few snapshots are kept alive so that the
    audio thread never frees memory when it swaps to the newest one. */
template <typename T>
class Published
{
public:
    void set (std::shared_ptr<const T> value)
    {
        juce::SpinLock::ScopedLockType l (lock);
        pending = std::move (value);
        keepAlive[keepIndex++ % keepAlive.size()] = pending;
    }

    /** Audio thread. */
    const std::shared_ptr<const T>& get()
    {
        juce::SpinLock::ScopedTryLockType l (lock);
        if (l.isLocked() && pending != active)
            active = pending;
        return active;
    }

    /** Message thread. */
    std::shared_ptr<const T> peek() const
    {
        juce::SpinLock::ScopedLockType l (lock);
        return pending;
    }

private:
    mutable juce::SpinLock lock;
    std::shared_ptr<const T> pending, active;
    std::array<std::shared_ptr<const T>, 4> keepAlive;
    size_t keepIndex = 0;
};
} // namespace kerf
