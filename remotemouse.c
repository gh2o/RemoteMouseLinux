#include <math.h>
#include <time.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <netinet/ip.h>
#include <X11/extensions/XTest.h>

static Display *display;
static int serversock;
static int clientsock;
static int justclicked = 0;

static const float initial_accel = 2.0;
static const float max_accel = 8.0;
static const float accel_step = 1.5;
static const int vsq_threshold = 50;

static inline void assert (int x, const char *r)
{
	if (x)
		return;
	fprintf (stderr, "assertion failed: %s\n", r);
	exit (1);
}

static uint64_t gettime ()
{
	struct timespec ts;
	clock_gettime (CLOCK_MONOTONIC, &ts);
	return (uint64_t) ts.tv_sec * 1000 + (uint64_t) ts.tv_nsec / 1000000;
}

static void accelerate (int *dx, int *dy)
{
	static const float k = 7.5;
	static const float r = 0.30;
	static const float d = 2.0;
	//static const int mxm = 400;

	static uint64_t last_ts = 0;
	static float last_mult = 0.0;

	uint64_t ts = gettime ();
	int dts = ts - last_ts;
	last_ts = ts;

	if (justclicked)
	{
		justclicked = 0;
		*dx = 0;
		*dy = 0;
		return;
	}

	float vel = sqrtf (*dx * *dx + *dy * *dy);
	if (vel == 0)
		return;

	last_mult /= expf (d * dts);

	float mult = logf (vel) * k;
	mult = (mult * r) + (last_mult * (1.0 - r));
	mult = (mult > 1.0) ? mult : 1.0;
	last_mult = mult;

	*dx *= mult;
	*dy *= mult;
}

static int fullrecv (int sock, void *buf, size_t len)
{
	return recv (sock, buf, len, MSG_WAITALL) == len;
}

static void handle ()
{
	while (1)
	{
		char cmd[7];
		if (!fullrecv (clientsock, cmd, 6))
			break;
		cmd[6] = '\0';

		int dsize = strtol (cmd + 3, NULL, 10);
		assert (dsize > 0, "invalid command data size");
		cmd[3] = '\0';

		char data[dsize + 1];
		if (!fullrecv (clientsock, data, dsize))
			break;
		data[dsize] = '\0';

		char sdata[dsize + 1];

		// assign words
		const int maxwords = 8;
		int nwords = 0;
		char *swords[maxwords];
		int iwords[maxwords];

		{
			memcpy (sdata, data, dsize + 1);

			int i;
			for (i = 0; i < maxwords; i++)
			{
				char *q = strtok (i ? NULL : sdata, " ");
				if (!q)
					break;

				swords[i] = q;
				iwords[i] = strtol (q, NULL, 10);
				nwords++;
			}

			if (strtok (NULL, " "))
			{
				printf ("too many args\n");
				continue;
			}
		}

		if (strcmp (cmd, "mos") == 0)
		{
			if (nwords < 1)
			{
				printf ("empty mouse command!");
				continue;
			}

			switch (*swords[0])
			{
				case 'm':
					if (nwords == 3)
					{
						int dx = iwords[1], dy = iwords[2];
						accelerate (&dx, &dy);
						XTestFakeRelativeMotionEvent (display, dx, dy, 0);
					}
					else
						printf ("bad mouse move command\n");
					break;
				case 'c':
					XTestFakeButtonEvent (display, 1, 1, 0);
					XTestFakeButtonEvent (display, 1, 0, 0);
					break;
				case 'R':
					if (nwords == 3)
					{
						int button = -1;
						switch (*swords[1])
						{
							case 'l':
								button = 1;
								break;
							case 'm':
								button = 2;
								break;
							case 'r':
								button = 3;
								break;
						}

						if (button < 0)
						{
							printf ("unknown mouse button %s\n", swords[1]);
							break;
						}

						int pressed = -1;
						switch (*swords[2])
						{
							case 'd':
								pressed = 1;
								break;
							case 'u':
								pressed = 0;
								break;
						}

						if (pressed < 0)
						{
							printf ("unknown mouse press %s\n", swords[2]);
							break;
						}

						XTestFakeButtonEvent (display, button, pressed, 0);
					}
					else
						printf ("bad mouse click command\n");
					break;
				case 'w':
					if (nwords == 2)
					{
						int button = -1;
						switch (*swords[1])
						{
							case '0':
								button = 4;
								break;
							case '1':
								button = 5;
								break;
						}

						if (button < 0)
						{
							printf ("unknown mouse scroll %s\n", swords[1]);
							break;
						}

						XTestFakeButtonEvent (display, button, 1, 0);
						XTestFakeButtonEvent (display, button, 0, 0);
					}
					else
						printf ("bad mouse scroll command\n");
					break;
				case 'b':
				case 's':
					justclicked = 1;
					break;
				default:
					printf ("unknown mouse command: %s\n", data);
					break;
			}
		}
		else
			printf ("unknown command: \"%s\"\n", cmd);

		XFlush (display);
	}
}

int main ()
{
	display = XOpenDisplay (NULL);
	assert (display != NULL, "cannot open display");

	serversock = socket (AF_INET, SOCK_STREAM, IPPROTO_TCP);
	assert (serversock >= 0, "cannot open socket");

	int n = 1;
	setsockopt (serversock, SOL_SOCKET, SO_REUSEADDR, &n, sizeof (n));

	struct sockaddr_in sa = {
		.sin_family = AF_INET,
		.sin_port = htons (1978),
		.sin_addr = { INADDR_ANY },
	};
	assert (bind (serversock, (struct sockaddr *) &sa, sizeof (sa)) == 0, "cannot bind");
	assert (listen (serversock, 4) == 0, "cannot listen");

	while (1)
	{
		printf ("waiting ...\n");
		clientsock = accept (serversock, NULL, NULL);
		assert (clientsock >= 0, "cannot accept");
		printf ("accepted!\n");
		handle ();
		close (clientsock);
	}

	return 0;
}
