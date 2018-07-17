#define _GNU_SOURCE
#include <json-c/json.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/timerfd.h>

#define AFB_BINDING_VERSION 2
#include <afb/afb-binding.h>

static pthread_mutex_t mutex;
static double loadavg[3];
static struct afb_event load_event;

static void *load_thread(void *arg)
{
	int timer_fd;
	unsigned long long missed;
	ssize_t ret;
	unsigned int sec = 5;
	unsigned int ns = 0;
	struct itimerspec itval;
	json_object *event, *load;

	timer_fd = timerfd_create(CLOCK_MONOTONIC, 0);
	if (timer_fd == -1)
		return NULL;

	itval.it_interval.tv_sec = sec;
	itval.it_interval.tv_nsec = ns;
	itval.it_value.tv_sec = sec;
	itval.it_value.tv_nsec = ns;
	ret = timerfd_settime(timer_fd, 0, &itval, NULL);

	while (1) {
		ret = read(timer_fd, &missed, sizeof(missed));
		if (ret == -1) {
			AFB_ERROR("read timer");
			continue;
		}

		event = json_object_new_object();
		pthread_mutex_lock(&mutex);
		getloadavg(loadavg, 3);
		load = json_object_new_double(loadavg[0]);
		pthread_mutex_unlock(&mutex);
		json_object_object_add(event, "value", load);
		afb_event_push(load_event, event);
	}

	return NULL;
}

int init()
{
	pthread_t t_load;

	AFB_NOTICE("load binding init");

	load_event = afb_daemon_make_event("load");

	pthread_create(&t_load, NULL, load_thread, NULL);

	return 0;
}

static void load(struct afb_req request)
{
	struct json_object *response, *load;

	response = json_object_new_object();
	pthread_mutex_lock(&mutex);
	load = json_object_new_double(loadavg[0]);
	pthread_mutex_unlock(&mutex);
	json_object_object_add(response, "value", load);

	afb_req_success(request, response, NULL);
}

static void subscribe(struct afb_req request)
{
	const char *value = afb_req_value(request, "value");

	if (!value) {
		afb_req_fail(request, "failed", "No event");
		return;
	}

	if (!strcmp(value, "load")) {
		afb_req_subscribe(request, load_event);
		afb_req_success(request, NULL, NULL);
	} else {
		afb_req_fail(request, "failed", "Invalid event");
	}
}

static void unsubscribe(struct afb_req request)
{
	const char *value = afb_req_value(request, "value");

	if (!value) {
		afb_req_fail(request, "failed", "No event");
		return;
	}

	if (value) {
		if (!strcmp(value, "load")) {
			afb_req_unsubscribe(request, load_event);
			afb_req_success(request, NULL, NULL);
		} else {
			afb_req_fail(request, "failed", "Invalid event");
		}
	}
}

static const struct afb_verb_v2 binding_verbs[] = {
	{ .verb = "load",	.callback = load,	.info = "Get load average" },
	{ .verb = "subscribe",	.callback = subscribe,	.info = "Subscribe to events" },
	{ .verb = "unsubscribe",.callback = unsubscribe,.info = "Unsubscribe to events" },
	{ }
};

const struct afb_binding_v2 afbBindingV2 = {
	.api = "load",
	.specification = NULL,
	.init = init,
	.verbs = binding_verbs,
};
