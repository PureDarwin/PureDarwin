#include <SymptomReporter/SymptomReporter.h>

#include <os/log.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* IPConfiguration sets one (interface index); leave room for a few more so a
 * caller with several does not silently lose them. */
#define SYMPTOM_MAX_QUALIFIERS	8

struct symptom_framework {
	uint32_t	sf_id;
	char		*sf_text_id;
	uint32_t	sf_version;
	os_log_t	sf_log;
};

struct symptom {
	symptom_framework_t	s_framework;
	symptom_ident_t		s_ident;
	uint64_t		s_qualifiers[SYMPTOM_MAX_QUALIFIERS];
	bool			s_qualifier_set[SYMPTOM_MAX_QUALIFIERS];
	uint64_t		s_digest;
	bool			s_has_digest;
};

symptom_framework_t
symptom_framework_init(uint32_t id, const char *text_id)
{
	symptom_framework_t	framework;

	framework = calloc(1, sizeof(*framework));
	if (framework == NULL) {
		return NULL;
	}
	framework->sf_id = id;
	if (text_id != NULL) {
		framework->sf_text_id = strdup(text_id);
		if (framework->sf_text_id == NULL) {
			free(framework);
			return NULL;
		}
	}
	/* Subsystem is the reporter's own reverse-DNS name, so symptoms from
	 * different reporters stay distinguishable in the log. */
	framework->sf_log
		= os_log_create(framework->sf_text_id != NULL
				? framework->sf_text_id : "com.apple.symptoms",
				"symptoms");
	return framework;
}

int
symptom_framework_set_version(symptom_framework_t framework, uint32_t version)
{
	if (framework == NULL) {
		return -1;
	}
	framework->sf_version = version;
	return 0;
}

int
symptom_framework_stats(symptom_framework_t framework)
{
	/* Statistics are kept by symptomsd, which does not exist here. */
	return framework == NULL ? -1 : 0;
}

int
symptom_verbosity(symptom_framework_t framework, uint32_t level)
{
	/* os_log applies its own filtering, so there is no separate level. */
	(void)level;
	return framework == NULL ? -1 : 0;
}

symptom_t
symptom_new(symptom_framework_t framework, symptom_ident_t ident)
{
	symptom_t	symptom;

	if (framework == NULL) {
		return NULL;
	}
	symptom = calloc(1, sizeof(*symptom));
	if (symptom == NULL) {
		return NULL;
	}
	symptom->s_framework = framework;
	symptom->s_ident = ident;
	return symptom;
}

symptom_t
symptom_create(symptom_framework_t framework, symptom_ident_t ident)
{
	return symptom_new(framework, ident);
}

int
symptom_set_qualifier(symptom_t symptom, uint64_t value, uint32_t index)
{
	if (symptom == NULL || index >= SYMPTOM_MAX_QUALIFIERS) {
		return -1;
	}
	symptom->s_qualifiers[index] = value;
	symptom->s_qualifier_set[index] = true;
	return 0;
}

int
symptom_set_additional_qualifier(symptom_t symptom, uint32_t type,
				 size_t length, const void *value)
{
	/* Free-form payload for symptomsd to interpret; with no consumer there
	 * is nothing meaningful to record beyond the fact it was set. */
	(void)type;
	(void)length;
	(void)value;
	return symptom == NULL ? -1 : 0;
}

int
symptom_set_additional_digest(symptom_t symptom, uint64_t digest)
{
	if (symptom == NULL) {
		return -1;
	}
	symptom->s_digest = digest;
	symptom->s_has_digest = true;
	return 0;
}

/*
 * Both send calls consume the symptom, matching the framework's contract that a
 * symptom is not reusable after submission. The distinction the real framework
 * draws - queued versus delivered right away - has no meaning without a daemon
 * to queue for.
 */
static int
symptom_emit(symptom_t symptom)
{
	symptom_framework_t	framework;
	char			quals[SYMPTOM_MAX_QUALIFIERS * 24];
	char			digest[32];
	size_t			used = 0;

	if (symptom == NULL) {
		return -1;
	}
	framework = symptom->s_framework;
	quals[0] = '\0';
	digest[0] = '\0';
	for (uint32_t i = 0; i < SYMPTOM_MAX_QUALIFIERS; i++) {
		int	n;

		if (!symptom->s_qualifier_set[i]) {
			continue;
		}
		n = snprintf(quals + used, sizeof(quals) - used,
			     "%s%u=%llu", used == 0 ? "" : " ",
			     i, (unsigned long long)symptom->s_qualifiers[i]);
		if (n < 0 || (size_t)n >= sizeof(quals) - used) {
			break;
		}
		used += (size_t)n;
	}

	if (symptom->s_has_digest) {
		snprintf(digest, sizeof(digest), " digest %#llx",
			 (unsigned long long)symptom->s_digest);
	}

	os_log(framework->sf_log, "symptom %#x reporter %#x [%s]%s",
	       symptom->s_ident, framework->sf_id, quals, digest);

	free(symptom);
	return 0;
}

int
symptom_send(symptom_t symptom)
{
	return symptom_emit(symptom);
}

int
symptom_send_immediate(symptom_t symptom)
{
	return symptom_emit(symptom);
}
