/*
 * feederd - démon de contrôle du distributeur de croquettes.
 *
 * Reçoit des commandes texte sur un socket Unix et décide s'il faut
 * distribuer un repas. C'est lui qui applique les règles de sécurité :
 * la vision (Python) ne fait que *demander* un repas.
 *
 * Commandes (une par connexion, terminée par '\n') :
 *   REQUEST_MEAL -> "OK <grammes>" | "REFUSED <raison>" | "ERROR <raison>"
 *   STATUS       -> "meals=<n>/<max> last=<date|never> allowed=<yes|no ...>"
 */
#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>

#include "motor.h"

/* En dessous de cette année, l'horloge n'est pas à l'heure : le Pi n'a pas
 * de pile RTC et démarre en 1970 tant que NTP n'a pas synchronisé. */
#define MIN_VALID_YEAR 2025

struct config {
	const char *socket_path;
	const char *state_path;
	int max_meals;		/* repas maximum par jour */
	int min_interval_s;	/* délai minimum entre deux repas */
	unsigned int grams;	/* quantité par repas */
};

/* État persistant : sauvegardé sur disque pour survivre à un redémarrage,
 * sinon débrancher le distributeur remettrait le compteur à zéro. */
struct state {
	long day;		/* jour courant : année * 1000 + jour de l'année */
	int meals;		/* repas déjà servis ce jour-là */
	time_t last_meal;	/* horodatage du dernier repas, 0 si aucun */
};

static volatile sig_atomic_t running = 1;

static void on_signal(int sig)
{
	(void)sig;
	running = 0;
}

static void log_msg(const char *fmt, ...)
{
	char ts[32];
	time_t now = time(NULL);
	struct tm tm;
	va_list ap;

	localtime_r(&now, &tm);
	strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", &tm);
	fprintf(stderr, "[%s] ", ts);
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fputc('\n', stderr);
}

static long day_key(time_t t)
{
	struct tm tm;

	localtime_r(&t, &tm);
	return (tm.tm_year + 1900) * 1000L + tm.tm_yday;
}

static void state_load(const char *path, struct state *st)
{
	FILE *f = fopen(path, "r");
	long last = 0;

	memset(st, 0, sizeof(*st));
	if (!f)
		return;		/* premier démarrage : état vierge */
	if (fscanf(f, "%ld %d %ld", &st->day, &st->meals, &last) == 3)
		st->last_meal = (time_t)last;
	else
		memset(st, 0, sizeof(*st));
	fclose(f);
}

/* Écrit dans un fichier temporaire puis le renomme : rename() est atomique,
 * une coupure de courant ne laisse donc jamais un fichier à moitié écrit. */
static int state_save(const char *path, const struct state *st)
{
	char tmp[512];
	FILE *f;

	snprintf(tmp, sizeof(tmp), "%s.tmp", path);
	f = fopen(tmp, "w");
	if (!f)
		return -1;
	fprintf(f, "%ld %d %ld\n", st->day, st->meals, (long)st->last_meal);
	fflush(f);
	fsync(fileno(f));
	if (fclose(f) != 0)
		return -1;
	return rename(tmp, path);
}

/* Renvoie 1 si un repas est autorisé maintenant, sinon 0 et remplit reason. */
static int meal_allowed(const struct config *cfg, struct state *st, time_t now,
			char *reason, size_t len)
{
	long today = day_key(now);

	if (today / 1000 < MIN_VALID_YEAR) {
		snprintf(reason, len, "horloge non synchronisee");
		return 0;
	}
	if (st->day != today) {		/* nouveau jour : compteur à zéro */
		st->day = today;
		st->meals = 0;
	}
	if (st->meals >= cfg->max_meals) {
		snprintf(reason, len, "max %d repas/jour atteint", cfg->max_meals);
		return 0;
	}
	if (st->last_meal && now - st->last_meal < cfg->min_interval_s) {
		long wait = cfg->min_interval_s - (long)(now - st->last_meal);

		snprintf(reason, len, "trop tot, attendre %ld min", (wait + 59) / 60);
		return 0;
	}
	return 1;
}

static void handle_command(const struct config *cfg, struct state *st,
			   const char *cmd, char *reply, size_t len)
{
	time_t now = time(NULL);
	char reason[96];

	if (strcmp(cmd, "REQUEST_MEAL") == 0) {
		if (!meal_allowed(cfg, st, now, reason, sizeof(reason))) {
			log_msg("repas refuse : %s", reason);
			snprintf(reply, len, "REFUSED %s", reason);
			return;
		}
		/* On compte le repas AVANT de tourner le moteur : si le démon
		 * plante pendant la distribution, mieux vaut un repas manqué
		 * qu'un repas en trop. */
		st->meals++;
		st->last_meal = now;
		if (state_save(cfg->state_path, st) != 0)
			log_msg("attention : sauvegarde de l'etat impossible (%s)",
				strerror(errno));

		log_msg("repas %d/%d accepte (%u g)", st->meals, cfg->max_meals,
			cfg->grams);
		if (motor_dispense(cfg->grams) != 0) {
			log_msg("erreur moteur");
			snprintf(reply, len, "ERROR moteur");
			return;
		}
		snprintf(reply, len, "OK %u", cfg->grams);
	} else if (strcmp(cmd, "STATUS") == 0) {
		char last[32] = "never";
		int ok = meal_allowed(cfg, st, now, reason, sizeof(reason));
		struct tm tm;

		if (st->last_meal) {
			localtime_r(&st->last_meal, &tm);
			strftime(last, sizeof(last), "%Y-%m-%dT%H:%M", &tm);
		}
		snprintf(reply, len, "meals=%d/%d last=%s allowed=%s%s",
			 st->meals, cfg->max_meals, last,
			 ok ? "yes" : "no ", ok ? "" : reason);
	} else {
		snprintf(reply, len, "ERROR commande inconnue");
	}
}

static int server_open(const char *path)
{
	struct sockaddr_un addr;
	int fd;

	if (strlen(path) >= sizeof(addr.sun_path)) {
		errno = ENAMETOOLONG;
		return -1;
	}
	fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (fd < 0)
		return -1;

	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	strcpy(addr.sun_path, path);
	unlink(path);		/* socket laissé par une exécution précédente */

	if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0 ||
	    listen(fd, 4) < 0) {
		close(fd);
		return -1;
	}
	return fd;
}

/* Lit une ligne sans le '\n' final. Renvoie 0 si une ligne a été lue. */
static int read_line(int fd, char *buf, size_t len)
{
	size_t n = 0;

	while (n < len - 1) {
		if (read(fd, buf + n, 1) <= 0 || buf[n] == '\n')
			break;
		n++;
	}
	buf[n] = '\0';
	if (n > 0 && buf[n - 1] == '\r')
		buf[--n] = '\0';
	return n > 0 ? 0 : -1;
}

static void serve_client(const struct config *cfg, struct state *st, int fd)
{
	struct timeval tv = { .tv_sec = 2 };
	char cmd[128], reply[256];

	/* Un client qui ne dit rien ne doit pas bloquer le démon. */
	setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
	if (read_line(fd, cmd, sizeof(cmd)) == 0) {
		handle_command(cfg, st, cmd, reply, sizeof(reply));
		dprintf(fd, "%s\n", reply);
	}
	close(fd);
}

static void usage(const char *prog)
{
	fprintf(stderr,
		"usage: %s [-s socket] [-f fichier_etat] [-m repas_max]\n"
		"          [-i intervalle_minutes] [-g grammes]\n", prog);
}

int main(int argc, char **argv)
{
	struct config cfg = {
		.socket_path = "/tmp/feeder.sock",
		.state_path = "feederd.state",
		.max_meals = 2,
		.min_interval_s = 4 * 3600,
		.grams = 50,
	};
	struct state st;
	struct sigaction sa;
	int opt, srv;

	while ((opt = getopt(argc, argv, "s:f:m:i:g:h")) != -1) {
		switch (opt) {
		case 's': cfg.socket_path = optarg; break;
		case 'f': cfg.state_path = optarg; break;
		case 'm': cfg.max_meals = atoi(optarg); break;
		case 'i': cfg.min_interval_s = atoi(optarg) * 60; break;
		case 'g': cfg.grams = (unsigned int)atoi(optarg); break;
		default:
			usage(argv[0]);
			return opt == 'h' ? 0 : 1;
		}
	}

	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = on_signal;	/* sans SA_RESTART : interrompt accept() */
	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGTERM, &sa, NULL);
	sa.sa_handler = SIG_IGN;	/* un client parti trop tôt ne doit pas nous tuer */
	sigaction(SIGPIPE, &sa, NULL);

	state_load(cfg.state_path, &st);
	if (motor_init() != 0) {
		log_msg("initialisation du moteur impossible");
		return 1;
	}
	srv = server_open(cfg.socket_path);
	if (srv < 0) {
		log_msg("socket %s : %s", cfg.socket_path, strerror(errno));
		motor_cleanup();
		return 1;
	}

	log_msg("feederd pret sur %s (max %d repas/jour, intervalle %d min, %u g)",
		cfg.socket_path, cfg.max_meals, cfg.min_interval_s / 60, cfg.grams);

	while (running) {
		int fd = accept(srv, NULL, NULL);

		if (fd < 0) {
			if (errno != EINTR)
				log_msg("accept : %s", strerror(errno));
			continue;
		}
		serve_client(&cfg, &st, fd);
	}

	log_msg("arret");
	close(srv);
	unlink(cfg.socket_path);
	motor_cleanup();
	return 0;
}
