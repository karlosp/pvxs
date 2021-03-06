/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvxs is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#include <epicsAssert.h>
#include <epicsMutex.h>
#include <epicsGuard.h>

#include <deque>

#include <pvxs/log.h>
#include "clientimpl.h"

namespace pvxs {
namespace client {

typedef epicsGuard<epicsMutex> Guard;

DEFINE_LOGGER(monevt, "pvxs.client.monitor");
DEFINE_LOGGER(io, "pvxs.client.io");

namespace {
struct Entry {
    Value val;
    std::exception_ptr exc;
    Entry() = default;
    Entry(Value&& v) :val(std::move(v)) {}
    Entry(const std::exception_ptr& e) :exc(e) {}
};
}

struct SubscriptionImpl : public OperationBase, public Subscription
{
    // for use in log messages, event after cancel()
    const std::string channelName;

    evevent ackTick;

    // const after exec()
    std::function<void(Subscription&)> event;
    Value pvRequest;
    bool pipeline = false;
    bool autostart = true;
    bool maskConn = false, maskDiscon = true;
    uint32_t queueSize = 4u, ackAt=0u;

    // only access from tcp_loop

    enum state_t : uint8_t {
        Connecting, // waiting for an active Channel
        Creating,   // waiting for reply to INIT
        Idle,       // waiting for start
        Running,    // waiting for stop
        Done,       // Finished or error
    } state = Connecting;

    mutable epicsMutex lock;

    // guarded by lock

    std::deque<Entry> queue;
    uint32_t window =0u, unack =0u;

    INST_COUNTER(SubscriptionImpl);

    SubscriptionImpl(operation_t op, const std::shared_ptr<Channel>& chan)
        :OperationBase (op, chan)
        ,channelName(chan->name)
        ,ackTick(event_new(chan->context->tcp_loop.base, -1, EV_TIMEOUT, &tickAckS, this))
    {}
    virtual ~SubscriptionImpl() {
        chan->context->tcp_loop.assertInLoop();
        _cancel(true);
    }

    void notify()
    {
        log_info_printf(monevt, "Server %s channel '%s' monitor notify\n",
                        chan->conn ? chan->conn->peerName.c_str() : "<disconnected>",
                        chan->name.c_str());
        if(event) {
            try {
                event(*this);
            }catch(std::exception& e){
                log_exc_printf(io, "Unhandled user exception in Monitor %s %s : %s\n",
                                __func__, typeid (e).name(), e.what());
            }
        }
    }

    virtual void pause(bool p) override final
    {
        chan->context->tcp_loop.call([this, p](){
            log_info_printf(io, "Server %s channel %s monitor %s\n",
                            chan->conn ? chan->conn->peerName.c_str() : "<disconnected>",
                            chan->name.c_str(),
                            p ? "pause" : "resume");

            if((state==Idle && !p) || (state==Running && p)) {
                auto& conn = chan->conn;

                {
                    uint8_t subcmd = p ? 0x04 : 0x44; // STOP | START

                    (void)evbuffer_drain(conn->txBody.get(), evbuffer_get_length(conn->txBody.get()));

                    EvOutBuf R(hostBE, conn->txBody.get());

                    to_wire(R, chan->sid);
                    to_wire(R, ioid);
                    to_wire(R, subcmd);
                }
                conn->enqueueTxBody(CMD_MONITOR);

                state = p ? Idle : Running;
            }
        });
    }

    virtual Value pop() override final
    {
        Value ret;
        {
            Guard G(lock);

            if(!queue.empty()) {
                auto ent(queue.front());
                queue.pop_front();

                if(pipeline) {
                    timeval tick{}; // immediate ACK

                    // schedule delayed ack while below threshold.
                    // avoid overhead of re-scheduling when unack in range [1, ackAt)
                    if(unack==0u && ackAt!=1u)
                        tick = timeval{1,0};

                    if(unack==0u || unack>=ackAt) {
                        if(event_add(ackTick.get(), &tick))
                            log_err_printf(io, "Monitor '%s' unable to schedule ack\n", channelName.c_str());
                    }

                    unack++;
                }

                log_info_printf(monevt, "channel '%s' monitor pop() %s\n",
                                channelName.c_str(),
                                ent.exc ? "exception" : ent.val ? "data" : "null!");

                if(ent.exc)
                    std::rethrow_exception(ent.exc);
                else
                    ret = std::move(ent.val);

            } else {
                log_info_printf(monevt, "channel '%s' monitor pop() empty\n",
                                channelName.c_str());
            }
        }
        return ret;
    }

    virtual void cancel() override final {
        auto context = chan->context;
        decltype (event) junk;
        context->tcp_loop.call([this, &junk](){
            _cancel(false);
            junk = std::move(event);
            // leave opByIOID for GC
        });
    }

    void _cancel(bool implicit) {
        if(implicit && state!=Done) {
            log_info_printf(io, "Server %s channel %s monitor implied cancel\n",
                            chan->conn ? chan->conn->peerName.c_str() : "<disconnected>",
                            chan->name.c_str());
        }
        log_info_printf(io, "Server %s channel %s monitor cancel\n",
                        chan->conn ? chan->conn->peerName.c_str() : "<disconnected>",
                        chan->name.c_str());

        if(state==Idle || state==Running) {
            chan->conn->sendDestroyRequest(chan->sid, ioid);

            // This opens up a race with an in-flight reply.
            chan->conn->opByIOID.erase(ioid);
            chan->opByIOID.erase(ioid);

            if(pipeline)
                (void)event_del(ackTick.get());
        }
        state = Done;
    }

    virtual void createOp() override final
    {
        if(state!=Connecting) {
            return;
        }

        auto& conn = chan->conn;

        {
            uint8_t subcmd = 0x08; // INIT
            if(pipeline)
                subcmd |= 0x80;

            (void)evbuffer_drain(conn->txBody.get(), evbuffer_get_length(conn->txBody.get()));

            EvOutBuf R(hostBE, conn->txBody.get());

            to_wire(R, chan->sid);
            to_wire(R, ioid);
            to_wire(R, subcmd);
            to_wire(R, Value::Helper::desc(pvRequest));
            to_wire_full(R, pvRequest);
            if(pipeline)
                to_wire(R, queueSize);
        }
        conn->enqueueTxBody(CMD_MONITOR);

        log_debug_printf(io, "Server %s channel '%s' monitor INIT%s\n",
                         conn->peerName.c_str(), chan->name.c_str(), pipeline?" pipeline":"");

        state = Creating;

        bool empty = false;
        if(!maskConn || pipeline) {
            Guard G(lock);

            if(!maskConn) {
                empty = queue.empty();

                queue.emplace_back(Entry(std::make_exception_ptr(Connected(conn->peerName))));
            }
            if(pipeline)
                window = queueSize;
        }

        if(empty)
            notify();
    }

    virtual void disconnected(const std::shared_ptr<OperationBase> &self) override final
    {
        log_debug_printf(io, "Server %s channel %s monitor disconnected in %d\n",
                        chan->conn ? chan->conn->peerName.c_str() : "<disconnected>",
                        chan->name.c_str(),
                        state);

        switch (state) {
        case Connecting:
        case Done:
            // noop
            break;
        case Creating:
        case Idle:
        case Running:
            // return to pending

            bool empty = false;
            if(!maskDiscon) {
                Guard G(lock);
                empty = queue.empty();

                queue.emplace_back(Entry(std::make_exception_ptr(Disconnect())));
            }

            chan->pending.push_back(self);
            state = Connecting;

            if(empty)
                notify();

            break;
        }
    }

    void tickAck()
    {
        if(((state==Idle) || (state==Running)) && pipeline && unack) {
            log_debug_printf(io, "Server %s channel %s monitor ACK\n",
                            chan->conn ? chan->conn->peerName.c_str() : "<disconnected>",
                            chan->name.c_str());

            auto& conn = chan->conn;
            {
                (void)evbuffer_drain(conn->txBody.get(), evbuffer_get_length(conn->txBody.get()));

                EvOutBuf R(hostBE, conn->txBody.get());

                to_wire(R, chan->sid);
                to_wire(R, ioid);
                to_wire(R, uint8_t(0x80));
                to_wire(R, uint32_t(unack));
            }
            conn->enqueueTxBody(CMD_MONITOR);

            window += unack;
            unack = 0u;
        }
    }
    static
    void tickAckS(evutil_socket_t fd, short evt, void *raw)
    {
        try {
            static_cast<SubscriptionImpl*>(raw)->tickAck();
        }catch(std::exception& e) {
            log_exc_printf(io, "Unhandled exception in %s %s : %s\n",
                           __func__, typeid (e).name(), e.what());
        }
    }
};

void Connection::handle_MONITOR()
{
    EvInBuf M(peerBE, segBuf.get(), 16);

    uint32_t ioid=0;
    uint8_t subcmd=0;
    Status sts{};
    Value data; // hold prototype (INIT) or reply data

    from_wire(M, ioid);
    from_wire(M, subcmd);
    bool init = subcmd&0x08;
    bool final = subcmd&0x10;

    if(init || final)
        from_wire(M, sts);
    if(init)
        from_wire_type(M, rxRegistry, data);

    RequestInfo* info=nullptr;
    if(M.good()) {
        auto it = opByIOID.find(ioid);
        if(it!=opByIOID.end()) {
            info = &it->second;

        } else {
            auto lvl = Level::Debug;
            if(!init) {
                // We don't have enough information to decode the rest of the payload.
                // This *may* leave rxRegistry out of sync (if it contains Variant Unions).
                // We can't know whether this is the case.
                // Failing soft here may lead to failures decoding future replies.
                // We could force close the Connection here to be "safe".
                // However, we assume the such usage of Variant is relatively rare

                lvl = Level::Err;
            }

            log_printf(io, lvl,  "Server %s uses non-existant IOID %u.  Ignoring...\n",
                       peerName.c_str(), unsigned(ioid));
            return;
        }

        if(!sts.isSuccess()) {

        } else if(init) {
            info->prototype = std::move(data);

        } else if(!final || !M.empty()) {

            data = info->prototype.cloneEmpty();
            from_wire_valid(M, rxRegistry, data);

            BitMask overrun;
            from_wire(M, overrun);
            (void)overrun; // ignoring
        }
    }

    // validate received message against operation state

    std::shared_ptr<OperationBase> op;
    SubscriptionImpl* mon = nullptr;
    if(M.good() && info) {
        op = info->handle.lock();
        if(!op) {
            // assume op has already sent CMD_DESTROY_REQUEST
            log_debug_printf(io, "Server %s ignoring stake cmd%02x ioid %u\n",
                             peerName.c_str(), CMD_MONITOR, unsigned(ioid));
            return;
        }

        if(uint8_t(op->op)!=CMD_MONITOR) {
            // peer mixes up IOID and operation type
            M.fault();

        } else {
            mon = static_cast<SubscriptionImpl*>(op.get());

            // check that subcmd is as expected based on operation state
            if((mon->state==SubscriptionImpl::Creating) && init) {

            } else if((mon->state==SubscriptionImpl::Idle) && !init) {

            } else if((mon->state==SubscriptionImpl::Running) && !init) {

            } else {
                M.fault();
            }
        }
    }

    if(!M.good() || !mon) {
        log_crit_printf(io, "Server %s sends invalid MONITOR.  Disconnecting...\n", peerName.c_str());
        bev.reset();
        return;
    }

    Entry update;

    if(!sts.isSuccess()) {
        update.exc = std::make_exception_ptr(RemoteError(sts.msg));
        mon->state = SubscriptionImpl::Done;

    } else if(mon->state==SubscriptionImpl::Creating) {
        log_debug_printf(io, "Server %s channel %s monitor Created\n",
                        peerName.c_str(),
                        mon->chan->name.c_str());

        mon->state = SubscriptionImpl::Idle;

        if(mon->autostart)
            mon->resume();

    } else if(data) { // Idle or Running
        update.val = std::move(data);

    } else {
        // NULL update.  can this happen?
        log_debug_printf(io, "Server %s channel %s monitor RX NULL\n",
                        peerName.c_str(),
                        mon->chan->name.c_str());
    }

    bool notify = false;
    if(!init) {
        Guard G(mon->lock);

        if(mon->pipeline) {
            if(mon->window) {
                mon->window--;
            } else {
                log_err_printf(io, "Server %s channel '%s' MONITOR exceeds window size\n",
                                peerName.c_str(), mon->chan->name.c_str());
            }
        }

        notify = mon->queue.empty();

        if(update.exc || (mon->queue.size() < mon->queueSize) || mon->queue.back().exc) {
            log_debug_printf(io, "Server %s channel %s monitor PUSH\n",
                            peerName.c_str(),
                            mon->chan->name.c_str());

            mon->queue.emplace_back(std::move(update));

        } else if(update.val) {
            log_debug_printf(io, "Server %s channel %s monitor Squash\n",
                            peerName.c_str(),
                            mon->chan->name.c_str());

            mon->queue.back().val.assign(update.val);
        }

        if(final && !update.exc) {
            log_debug_printf(io, "Server %s channel %s monitor FINISH\n",
                            peerName.c_str(),
                            mon->chan->name.c_str());

            mon->queue.emplace_back(std::make_exception_ptr(Finished()));
        }

        if(mon->queue.empty()) {
            log_err_printf(io, "Server %s channel '%s' MONITOR empty update!\n",
                           peerName.c_str(), mon->chan->name.c_str());
            notify = false;
        }
    }

    if(mon->state==SubscriptionImpl::Done || final) {
        mon->state=SubscriptionImpl::Done;

        opByIOID.erase(ioid);
        mon->chan->opByIOID.erase(ioid);

        if(mon->pipeline)
            (void)event_del(mon->ackTick.get());

        if(!final)
            sendDestroyRequest(mon->chan->sid, ioid);
    }

    if(notify)
        mon->notify();
}


std::shared_ptr<Subscription> MonitorBuilder::exec()
{
    std::shared_ptr<Subscription> ret;

    ctx->tcp_loop.call([&ret, this]() {
        auto chan = Channel::build(ctx->shared_from_this(), _name);

        auto op = std::make_shared<SubscriptionImpl>(Operation::Monitor, chan);
        op->event = std::move(_event);
        op->pvRequest = _buildReq();
        op->maskConn = _maskConn;
        op->maskDiscon = _maskDisconn;

        auto options = op->pvRequest["record._options"];

        options["queueSize"].as<uint32_t>([&op](uint32_t Q) {
            if(Q>1)
                op->queueSize = Q;
        });

        (void)options["pipeline"].as(op->pipeline);

        auto ackAny = options["ackAny"];

        if(ackAny.type()==TypeCode::String) {
            auto sval = ackAny.as<std::string>();
            if(sval.size()>1 && sval.back()=='%') {
                try {
                    auto percent = parseTo<double>(sval);
                    if(percent>0.0 && percent<=100.0) {
                        op->ackAt = uint32_t(percent * op->queueSize);
                    } else {
                        throw std::invalid_argument("not in range (0%, 100%]");
                    }
                }catch(std::exception&){
                    log_warn_printf(monevt, "Error parsing as percent ackAny: \"%s\"\n", sval.c_str());
                }
            }

        }

        if(op->ackAt==0u){
            uint32_t count=0u;

            if(ackAny.as(count)) {
                op->ackAt = count;
            }
        }

        if(op->ackAt==0u){
            op->ackAt = op->queueSize/2u;
        }

        op->ackAt = std::max(1u, std::min(op->ackAt, op->queueSize));

        chan->pending.push_back(op);
        chan->createOperations();

        ret.reset(op.get(), [op](Subscription*) mutable {
            // on user thread
            auto temp(std::move(op));
            temp->chan->context->tcp_loop.call([&temp]() {
                // on worker
                try {
                    temp->_cancel(true);
                }catch(std::exception& e){
                    log_exc_printf(monevt, "Channel %s error in monitor cancel(): %s",
                                   temp->channelName.c_str(), e.what());
                }
                // ensure dtor on worker
                temp.reset();
            });
        });
    });

    return  ret;
}

} // namespace client
} // namespace pvxs
