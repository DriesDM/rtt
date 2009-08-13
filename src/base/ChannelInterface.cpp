#include "../internal/Channels.hpp"
#include "../os/Atomic.hpp"

using namespace RTT;

ChannelElementBase::ChannelElementBase()
    : input(0)

{
    oro_atomic_set(&refcount,0);
}

void ChannelElementBase::setOutput(shared_ptr output)
{
    this->output = output;
    output->input = this;
}

void ChannelElementBase::removeInput()
{ input = 0; }
void ChannelElementBase::removeOutput()
{ output = 0; }

void ChannelElementBase::disconnect(bool forward)
{
    if (forward)
    {
        shared_ptr output = this->output;
        if (output)
            output->disconnect(true);
    }
    else
    {
        shared_ptr input = this->input;
        if (input)
            input->disconnect(false);
    }
}

ChannelElementBase::shared_ptr ChannelElementBase::getInput()
{ return input; }
ChannelElementBase::shared_ptr ChannelElementBase::getOutput()
{ return output; }

void ChannelElementBase::clear()
{
    shared_ptr input_ = input;
    if (input_) input_->clear();
}

bool ChannelElementBase::signal() const
{
    shared_ptr output = this->output;
    if (output) return output->signal();
    return true;
}

void ChannelElementBase::ref()
{
    oro_atomic_inc(&refcount);
}

void ChannelElementBase::deref()
{
    if ( oro_atomic_dec_and_test(&refcount) ) delete this;
}

void RTT::intrusive_ptr_add_ref( ChannelElementBase* p )
{ p->ref(); }

void RTT::intrusive_ptr_release( ChannelElementBase* p )
{ p->deref(); }
