/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvxs is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#include <testMain.h>

#include <epicsUnitTest.h>

#include <pvxs/unittest.h>
#include <pvxs/log.h>
#include <evhelper.h>

using namespace pvxs;
namespace  {

struct my_special_error : public std::runtime_error
{
    my_special_error() : std::runtime_error("Special") {}
};

void test_call()
{
    testDiag("%s", __func__);

    evbase base("TEST");

    auto snap = instanceSnapshot();
    testEq(snap["evbase"], 1u);

    testOk1(!base.inLoop());

    {
        bool called = false;
        base.call([&called, &base]() {
            testDiag("in loop 1");
            called = true;
            testOk1(!!base.inLoop());
            base.assertInLoop();
        });
        testOk1(called==true);
    }

    {
        bool called = false;
        base.dispatch([&called]() {
            testDiag("in loop 2");
            called = true;
        });

        base.sync();
        testOk1(called==true);
    }

    try {
        base.call([](){
            testDiag("in loop 3");
            throw my_special_error();
        });
        testFail("Unexpected success");
    }catch(my_special_error&) {
        testPass("Caught expected exception");
    }catch(std::exception& e) {
        testFail("Caught wrong exception : %s \"%s\"", typeid(e).name(), e.what());
    }

}

void test_fill_evbuf()
{
    testDiag("%s", __func__);

    evbuf buf(evbuffer_new());

    {
        EvOutBuf M(true, buf.get());
        testEq(M.size(), 0u);

        for(uint32_t i : range(1024)) {
            to_wire(M, i);
        }
        testDiag("Extra %u", unsigned(M.size()));
        testOk1(!!M.good());

        // ~EvOutBuf flushes to backing buf
    }

    testEq(evbuffer_get_length(buf.get()), 4*1024u);

    {
        EvInBuf M(true, buf.get());
        testEq(M.size(), 0u);

        bool match = true;
        for(uint32_t expect : range(1024)) {
            uint32_t actual=0;
            from_wire(M, actual);
            if(actual!=expect) {
                testDiag("%08x == %08x", unsigned(expect), unsigned(actual));
                match = false;
                break; // only show first failure
            }
        }
        testOk1(!!match);
        testOk1(!!M.good());
        testEq(M.size(), 0u);
        testOk1(!M.refill(42)); // should be completely empty
    }

    testEq(evbuffer_get_length(buf.get()), 0u);
}

} // namespace

MAIN(testev)
{
    SockAttach attach;
    testPlan(15);
    testSetup();
    test_call();
    test_fill_evbuf();
    cleanup_for_valgrind();
    return testDone();
}
