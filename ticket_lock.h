struct ticket_lock
{
	int ticket;
	int turn;
	int pid;
	// For debugging:
	char *name;        // Name of lock.
};
