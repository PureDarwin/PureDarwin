#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

struct node {
	struct node	*next;
	int		value;
};

static void
list_append(struct node **list, int value)
{
	assert(list);

	if (*list == NULL) {
		*list = malloc(sizeof(struct node));
		(*list)->value = value;
		(*list)->next  = NULL;
	} else {
		struct node *n = *list;

		while (n->next != NULL)
			n = n->next;

		n->next = malloc(sizeof(struct node));
		n->next->value = value;
		n->next->next  = NULL;

	}
}

static void
list_manipulate(struct node *list)
{
	while (list) {
		list->value += 1;
		list->value -= 1;
		list = list->next;
	}
}

int
main(void)
{
	struct node *my_list = NULL;

	list_append(&my_list, 0);
	list_append(&my_list, 1);
	list_append(&my_list, 1);
	list_append(&my_list, 2);
	list_append(&my_list, 3);
	list_append(&my_list, 5);
	list_append(&my_list, 8);
	list_append(&my_list, 13);

	for (;;) {
		list_manipulate(my_list);
		sleep(1);
	}

	return 0;
}

