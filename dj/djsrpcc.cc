dj_delivery_code
dj_rpc_call(gate_sender *gs, const dj_pubkey &node, time_t timeout,
            const dj_delegation_set &dset, const dj_catmap &cm,
            const dj_message &m, dj_message *reply)
{
    uint64_t timeout_at_msec = sys_clock_msec() + timeout * 1000;
    return DELIVERY_TIMEOUT;
}

