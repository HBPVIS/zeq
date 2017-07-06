Subscription Monitor {#monitor}
============

To enable an application to republish all its state when a new subscriber
connected, we need a way to monitor connection requests. This RFC describes this
feature for this use case, based on zmq_socket_monitor.

## API

    class Sender
    {
        virtual void* getSocket() = 0;
    };

    class Publisher : public Sender {};

    class Monitor : public Receiver
    {
        Monitor( Sender& sender );
        Monitor( Sender& sender, Receiver& shared );

        virtual void notifyNewConnection() {}
        // other notifies and params lazy, on use case
    };

## Examples

   Livre Communicator derives from Monitor and republishes all events on notify.


## Implementation

Monitor installs zmq_socket_monitor() on Sender::getSocket(). Monitor::process()
calls Monitor::notifyNewConnection().
