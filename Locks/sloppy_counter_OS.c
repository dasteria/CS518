typedef struct __counter_t
{
	int 					global;  			//	global counter
	pthread_mutex_T 		glock;				//	global counter lock
	int						local[NUMCPUS];		//	local count (per cpu)
	pthread_mutex_T			llock[NUMCPUS];		// 	local count locks
	int						threshold;			// 	cap of single cpu counter; related to update frequency
}	counter_t;


/*
	set threshold and global counter
	initialize locks for global counter and all local counters
	determined by number of cpus defined.
*/
void init(counter_t *c, int threshold)
{
	c->threshold = threshold;
	c->global = 0;
	pthread_mutex_init(&c->glock,NULL);
	int i;
	for(i=0;i<NUMCPUS;i++)
	{
		c->local[i] = 0;
		pthread_mutex_init(&c->llock[i],NULL);
	}
}
/*
	increse counter for particular threadID 
	by specified amonut.
	if threshold reached, it would update global
	counter and empty itself.
*/

void update(counter_t *c, int threadID, int amt)
{
	// Start operating on particular thread
	pthread_mutex_lock(&c->llock[threadID]);
	// Local counter increased by amount specified
	c->local[threadID] += amt;

	if(c->local[threadID] >= c->threshold)
	{
		pthread_mutex_lock(&c->glock);
		c->global += c->local[threadID];
		pthread_mutex_unlock(&c->glock);
		c->local[threadID] = 0;
	}

	pthread_mutex_unlock(&c->llock[threadID]);
}

/*
	return global counter. keep in mind, the global
	counter is just an estimate of the sum of all
	thread counters. 
*/

int get(counter_t *c)
{
	pthread_mutex_lock(&c->glock);
	int val = c->global;
	pthread_mutex_unlock(&c->glock);
	return val;
}
