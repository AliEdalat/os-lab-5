struct rwt_lock{
    struct ticket_lock mutex, wrt;
    int readcount;
};  