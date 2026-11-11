/* C HAL contracts exercised against the copied C++ modem, with a framed fake peer. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "ink/hal_cellular.h"
#include "ink/hal_device.h"
#include "ink/hal_net.h"
#include "ink/osal.h"
#include "s31_cell.h"
#include "cell_fixture.h"
static void drain_urcs(void) { char s[512]; while (hal_modem_urc(s,sizeof s,0)>0) {} }
static void at_ok(const char *cmd) {
    char s[512]; hal_at_result_t r=hal_modem_at(cmd,s,sizeof s,1000);
    if (r!=HAL_AT_OK) fprintf(stderr,"AT check result=%d rate=%u probes=%u writes=%u\n%s",r,
        ink_test_count(N_BAUD),ink_test_count(N_PROBES),ink_test_count(N_WRITES),ink_test_logs());
    assert(r==HAL_AT_OK);
}
int main(int argc,char **argv) {
    const char *mode=argc>1 ? argv[1] : "protocol";
    ink_test_set(F_SPLIT,256); ink_test_set(F_BAUD,2000000); ink_test_set(F_CSQ,23);
    if (!strcmp(mode,"baud")) ink_test_set(F_BAUD,3000000);
    if (!strcmp(mode,"dhcp")) ink_test_set(F_NO_DHCP,1);
    if (!strcmp(mode,"nat")) ink_test_set(F_BAD_NAT,1);
    if (!strcmp(mode,"started") || !strcmp(mode,"silent") || !strcmp(mode,"dhcp")) ink_test_set(F_STARTED,1);
    if (!strcmp(mode,"silent")) ink_test_set(F_SILENT_GREETING,1);
    ink_s31_cell_init(); ink_s31_cell_netif_init();
    char s[1024];
    if (strcmp(mode,"protocol")) ink_s31_cell_use(true);
    if (!strcmp(mode,"protocol")) { ink_test_set(F_DROP_PROBES,2); ink_test_set(F_SPLIT,1); }
    if (!strcmp(mode,"detect")) {
        ink_test_set(F_NO_RESPONSE,1);
        assert(hal_modem_at("ATI",s,sizeof s,1000)==HAL_AT_NO_MODEM);
        ink_test_set(F_NO_RESPONSE,0); ink_test_advance(5001); at_ok("AT");
        goto finish;
    }
    hal_at_result_t first_at=hal_modem_at("ATI",s,sizeof s,1000);
    if (first_at!=HAL_AT_OK) fprintf(stderr,"first AT result=%d rate=%u probes=%u\n%s",first_at,ink_test_count(N_BAUD),ink_test_count(N_PROBES),ink_test_logs());
    assert(first_at==HAL_AT_OK && !strcmp(s,"FAKE-NT26"));
    if (!strcmp(mode,"nat") || !strcmp(mode,"baud")) {
        assert(!ink_test_count(N_FORBIDDEN));
        assert(strstr(ink_test_logs(),"config_readonly") && strstr(ink_test_logs(),"Network wait failed") && !ink_test_count(N_HANDSHAKES));
        if (!strcmp(mode,"baud")) assert(ink_test_count(N_BAUD)==3000000);
        goto finish;
    }
    if (!strcmp(mode,"privacy")) {
        hal_cell_status_t status; assert(hal_cell_status(&status));
        ink_test_set(F_NO_RESPONSE,1);
        assert(hal_modem_at("ATD00000000000;",s,sizeof s,200)==HAL_AT_TIMEOUT);
        ink_test_set(F_NO_RESPONSE,0);
        const char *logs=ink_test_logs();
        assert(!strstr(logs,"00:11:22:33:44:55") && !strstr(logs,"123456789012345") && !strstr(logs,"00000000000"));
        assert(!strstr(logs,"1A2B") && !strstr(logs,"0C3D4E5F"));
        goto finish;
    }
    if (!strcmp(mode,"silent") || !strcmp(mode,"throttle") || !strcmp(mode,"dhcp")) {
        hal_cell_status_t status;
        hal_link_status_t link;
        unsigned tasks=ink_test_count(N_TASKS), radios=ink_test_count(N_RADIO);
        if (!strcmp(mode,"silent")) {
            assert(ink_test_count(N_HANDSHAKES)==1 && radios==3);
            ink_test_advance(5001); assert(hal_cell_status(&status));
            assert(ink_test_count(N_HANDSHAKES)==2 && ink_test_count(N_RADIO)==radios+1);
            ink_test_set(F_SILENT_GREETING,0); ink_test_advance(5001);
            assert(hal_cell_status(&status));
        } else if (!strcmp(mode,"throttle")) {
            ink_test_ip(true); ink_test_set(F_INTERLEAVE,1); at_ok("AT+CEREG?");
            assert(hal_cell_status(&status)); assert(ink_test_count(N_TASKS)==tasks);
            ink_test_advance(5001); assert(hal_cell_status(&status));
            assert(ink_test_count(N_TASKS)>tasks);
        } else {
            ink_test_advance(60001); ink_test_rebuild_status();
            assert(ink_test_count(N_TASKS)>tasks);
        }
        ink_test_ip(true); assert(ink_s31_cell_link(&link) && link.up);
        goto finish;
    }
    drain_urcs();
    assert(ink_test_count(N_WAKE_LEVEL)==5 && ink_test_count(N_TASKS)==3);
    if (!strcmp(mode,"protocol")) {
        assert(ink_test_count(N_DROPPED_PROBES)==2 && ink_test_count(N_BAUD)==2000000);
        ink_test_ethernet(); ink_test_drain(); assert(!ink_test_count(N_RX));
    }
    assert(hal_modem_at("ATI",s,sizeof s,1000)==HAL_AT_OK && !strcmp(s,"FAKE-NT26"));
    assert(hal_modem_urc(s,sizeof s,0)>0 && !strcmp(s,"+CMTI: \"SM\",7"));
    ink_test_set(F_SPLIT,5); unsigned split_loans=ink_test_count(N_LOANS);
    at_ok("AT"); ink_test_drain();
    assert(ink_test_count(N_LOANS)==split_loans+2 && ink_test_count(N_LOANS)==ink_test_count(N_RETURNS));
    ink_test_set(F_SPLIT,1);
    assert(hal_modem_at_payload("AT+CMGS=1","00",s,sizeof s,1000)==HAL_AT_OK && !strcmp(s,"+CMGS: 1"));
    ink_test_drain(); assert(!ink_test_count(N_HELD)); ink_test_set(F_SPLIT,256);
    assert(ink_test_gap_during_at()); ink_test_drain();
    assert(hal_modem_at("AT",s,sizeof s,200)==HAL_AT_NO_MODEM);
    ink_test_reply("OK\r\n"); ink_test_drain(); at_ok("AT");
    ink_test_reply("\r\n+CMTI: \"SM\",8\r\n"); ink_test_drain();
    assert(hal_modem_urc(s,sizeof s,0)>0 && !strcmp(s,"+CMTI: \"SM\",8"));
    unsigned sms_writes=ink_test_count(N_WRITES); ink_test_set(F_NO_PDU_FINAL,1);
    assert(hal_modem_at_payload("AT+CMGS=1","00",s,sizeof s,1000)==HAL_AT_TIMEOUT);
    assert(ink_test_count(N_WRITES)==sms_writes+2);
    ink_test_set(F_NO_PDU_FINAL,0); ink_test_reply("\r\nOK\r\n"); ink_test_drain(); at_ok("AT");
    const char *forbidden[]={"AT+ECSIMCFG=SimSlot,2","AT+ECSIMCFG=SimPowerSave,0","AT+ECSIMCFG=SimSlot,1;&W","AT+CFUN=0;&W",
        "AT+CGDCONT=1,\"IP\",\"example.com\"","AT+CGAUTH=1,0","AT+CMGD=7","AT&W","AT+ECRST","AT+ECNETCFG=1",
        "AT+XJCFG=netPortBaudRate,1","AT+IMS=1","AT+CFUN=1;&W","AT\r&W","ATD1;&W","AT+VTS=\"1\";&W",
        "AT+ECPCFG=\"usbCtrl\",1","AT+ECNETCFG=\"nat\",1,\"192.0.2.2\"","AT+ECSCLKEX=1,3,30"};
    unsigned before=ink_test_count(N_WRITES);
    for (unsigned i=0;i<sizeof forbidden/sizeof *forbidden;i++) assert(hal_modem_at(forbidden[i],s,sizeof s,1000)==HAL_AT_ERROR);
    hal_cell_apn_t apn={.name="example.com"}; assert(hal_cell_apn(&apn)<0);
    assert(hal_modem_at_payload("AT+CMGS=1","00\032AT&W",s,sizeof s,1000)==HAL_AT_ERROR);
    assert(ink_test_count(N_WRITES)==before);
    assert(hal_cell_dial("00000000000")==0 && ink_test_bh());
    assert(hal_cell_answer()==0 && ink_test_bh());
    assert(hal_cell_hangup()==0 && !ink_test_bh() && hal_cell_dtmf('5')==0);
    drain_urcs(); hal_cell_event_t event; hal_cell_wait_event(&event,0);
    ink_test_reply("\r\nNO CARRIER\r\n"); ink_test_drain();
    assert(hal_cell_wait_event(&event,0)==1 && event.type==HAL_CELL_EV_CALL_ENDED && !ink_test_bh());
    assert(hal_modem_at("AT+ECNETCFG?",s,sizeof s,1000)==HAL_AT_OK && strlen(s)>96 && strstr(s,"lease"));
    assert(hal_modem_at("AT+ECNETCFG?",s,32,1000)==HAL_AT_ERROR);
    ink_test_set(F_NO_ACK,1); before=ink_test_count(N_WRITES); at_ok("AT"); assert(ink_test_count(N_WRITES)==before+1); ink_test_set(F_NO_ACK,0);
    unsigned stops=ink_test_count(N_STOPS), starts=ink_test_count(N_STARTS);
    ink_test_set(F_NO_WAKE,1); ink_test_advance(501); ink_test_wait_count(N_STOPS,stops+1);
    before=ink_test_count(N_WRITES); at_ok("AT");
    assert(ink_test_count(N_WRITES)==before+1 && strstr(ink_test_logs(),"forcing active state"));
    ink_test_set(F_NO_WAKE,0); ink_test_wait_count(N_STARTS,starts+1); at_ok("AT");
    hal_cell_status_t status;
    ink_s31_cell_use(true); ink_test_advance(5001);
    assert(hal_cell_status(&status) && status.registered && status.signal_dbm==-67 && status.config_readonly && !status.sim_missing);
    assert(strstr(ink_test_logs(),"FAKE_R1 ###############") && !strstr(ink_test_logs(),"123456789012345"));
    assert(!strstr(ink_test_logs(),"1A2B") && !strstr(ink_test_logs(),"0C3D4E5F"));
    assert(strstr(ink_test_logs(),"cell: AT+CIREG? -> OK"));
    assert(hal_cell_status(&status));
    { unsigned n=0; for (const char *q=ink_test_logs();(q=strstr(q,"cell: AT+CGMM"));q++) n++; assert(n==1); }
    assert(!ink_test_count(N_FORBIDDEN));
    assert(!strstr(ink_test_logs(),"00:11:22:33:44:55"));
    if (!strcmp(mode,"dhcp")) {
        unsigned tasks=ink_test_count(N_TASKS); ink_test_advance(60001);
        assert(hal_cell_status(&status));
        assert(ink_test_count(N_TASKS)>tasks); /* no address: rebuild, never invent one */
    }
    ink_test_ip(true); hal_link_status_t link;
    assert(ink_s31_cell_link(&link) && link.up && link.signal_dbm==-67 && ink_s31_cell_available());
    assert(ink_test_transmit()); ink_test_wait_count(N_TX,1);
    ink_test_ethernet(); ink_test_drain(); assert(ink_test_count(N_RX)==1);
    ink_test_set(F_DROP_RX,1); ink_test_ethernet(); ink_test_drain(); ink_test_set(F_DROP_RX,0);
    uint64_t in,out; assert(hal_net_lte_bytes(&in,&out) && in==60 && out==60 && ink_test_count(N_RX)==1);
    unsigned ack_before=ink_test_count(N_ACKS);
    ink_test_set(F_CORRUPT,1); ink_test_ethernet(); ink_test_drain(); ink_test_set(F_CORRUPT,0); assert(ink_test_count(N_RX)==1 && ink_test_count(N_ACKS)==ack_before);
    drain_urcs(); ink_test_reply("\r\nRING\r\n+CLIP: \"00000000000\",129\r\nNO CARRIER\r\n"); ink_test_drain();
    assert(hal_modem_urc(s,sizeof s,0)>0 && !strcmp(s,"RING"));
    assert(hal_modem_urc(s,sizeof s,0)>0 && strstr(s,"+CLIP:"));
    assert(hal_modem_urc(s,sizeof s,0)>0 && !strcmp(s,"NO CARRIER"));
    ink_test_set(F_INTERLEAVE,1); at_ok("AT+CEREG?");
    assert(ink_s31_cell_link(&link) && !link.up);
    assert(hal_modem_urc(s,sizeof s,0)>0 && !strcmp(s,"+ECNETDEVCTL: 0"));
    assert(hal_modem_urc(s,sizeof s,0)>0 && !strcmp(s,"+ECNETDEVCTL: 1"));
    ink_test_advance(5001); assert(hal_cell_status(&status)); ink_test_ip(true);
    assert(ink_s31_cell_link(&link) && link.up);
    assert(hal_net_lte_bytes(&in,&out) && in==60 && out==60);
    before=ink_test_count(N_WRITES); ink_test_set(F_NO_RESPONSE,1);
    assert(hal_modem_at("ATD00000000000;",s,sizeof s,200)==HAL_AT_TIMEOUT && ink_test_count(N_WRITES)==before+1);
    ink_test_set(F_NO_RESPONSE,0); ink_test_reply("\r\nOK\r\n"); ink_test_drain(); at_ok("AT");
    before=ink_test_count(N_WRITES); ink_test_set(F_NO_RESPONSE,1);
    assert(hal_modem_at("ATI",s,sizeof s,200)==HAL_AT_TIMEOUT && ink_test_count(N_WRITES)==before+1);
    ink_test_set(F_NO_RESPONSE,0); assert(hal_modem_at("AT",s,sizeof s,200)==HAL_AT_NO_MODEM);
    ink_test_reply("\r\nOK\r\n"); ink_test_drain(); at_ok("AT");
    ink_test_set(F_NO_RESPONSE,1); assert(hal_modem_at("ATI",s,sizeof s,200)==HAL_AT_TIMEOUT); ink_test_set(F_NO_RESPONSE,0);
    assert(hal_modem_at("AT",s,sizeof s,200)==HAL_AT_NO_MODEM); ink_test_advance(30001); at_ok("AT");
    drain_urcs(); ink_test_reply("\r\nECRDY\r\n"); ink_test_drain();
    assert(ink_s31_cell_link(&link) && !link.up && !ink_s31_cell_available());
    ink_test_advance(5001); assert(hal_cell_status(&status)); ink_test_ip(true);
    assert(ink_s31_cell_link(&link) && link.up);
    unsigned radios=ink_test_count(N_RADIO);
    assert(hal_cell_select_sim(HAL_CELL_SLOT_INTERNAL,5000)==0 && ink_test_count(N_SLOT)==1 && ink_test_count(N_RADIO)==radios+2);
    assert(hal_cell_status(&status) && status.slot==HAL_CELL_SLOT_INTERNAL);
    assert(hal_cell_select_sim(HAL_CELL_SLOT_EXTERNAL,5000)==0 && ink_test_count(N_SLOT)==0 && hal_cell_status(&status) && status.slot==HAL_CELL_SLOT_EXTERNAL);
    ink_test_set(F_UNREGISTERED,1); ink_test_set(F_MISSING,1); ink_test_set(F_CSQ,99);
    assert(hal_cell_status(&status) && !status.registered && status.sim_missing && status.signal_dbm==0 && !ink_s31_cell_available());
    assert(ink_s31_cell_link(&link) && !link.up);
finish:
    ink_test_drain(); assert(ink_s31_cell_suspend());
    before=ink_test_count(N_WRITES);
    assert(hal_modem_at("AT",s,sizeof s,200)==HAL_AT_NO_MODEM && ink_test_count(N_WRITES)==before);
    assert(ink_test_count(N_LOANS)==ink_test_count(N_RETURNS) && !ink_test_count(N_HELD));
    assert(!ink_test_count(N_BAD_HEADERS) && !ink_test_count(N_FORBIDDEN) && ink_test_count(N_CHANGED));
    ink_s31_cell_resume();
    assert(hal_modem_at("AT",s,sizeof s,200)==HAL_AT_NO_MODEM); /* P14's one-second settle cannot fit */
    ink_test_advance(1001); ink_test_set(F_MISSING,0); ink_test_set(F_UNREGISTERED,0); ink_test_set(F_BAUD,3000000);
    unsigned probes=ink_test_count(N_PROBES);
    at_ok("AT"); assert(ink_test_count(N_PROBES)==probes+10 && ink_test_count(N_BAUD)==3000000 && !ink_test_count(N_FORBIDDEN));
    assert(ink_s31_cell_suspend()); ink_test_drain();
    assert(ink_test_count(N_LOANS)==ink_test_count(N_RETURNS) && !ink_test_count(N_HELD));
    ink_test_finish();
    puts("cell: PASS vendor tasks, AT/payload, URC, privacy, policy, lifecycle");
    return 0;
}
