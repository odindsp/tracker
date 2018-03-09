
#include "adx_rbtree.h"

void adx_rbtree_set_parent(adx_rbtree_node *rb, adx_rbtree_node *p)
{
	rb->rb_parent_color = (rb->rb_parent_color & 3) | (unsigned long)p;
}

void adx_rbtree_set_color(adx_rbtree_node *rb, int color)
{
	rb->rb_parent_color = (rb->rb_parent_color & ~1) | color;
}

void adx_rbtree_link_node(adx_rbtree_node *node, adx_rbtree_node *parent, adx_rbtree_node **rb_link)
{

	node->rb_parent_color = (unsigned long )parent;
	node->rb_left = node->rb_right = NULL;
	*rb_link = node;
}

void adx_rbtree_rotate_left(adx_rbtree_node *node, adx_rbtree_head *head)
{
	adx_rbtree_node *right = node->rb_right;
	adx_rbtree_node *parent = rb_parent(node);

	if ((node->rb_right = right->rb_left))
		adx_rbtree_set_parent(right->rb_left, node);

	right->rb_left = node;
	adx_rbtree_set_parent(right, parent);

	if (parent) {

		if (node == parent->rb_left)
			parent->rb_left = right;
		else
			parent->rb_right = right;

	} else {

		head->rb_node = right;
	}

	adx_rbtree_set_parent(node, right);
}

void adx_rbtree_rotate_right(adx_rbtree_node *node, adx_rbtree_head *head)
{
	adx_rbtree_node *left = node->rb_left;
	adx_rbtree_node *parent = rb_parent(node);

	if ((node->rb_left = left->rb_right))
		adx_rbtree_set_parent(left->rb_right, node);

	left->rb_right = node;
	adx_rbtree_set_parent(left, parent);

	if (parent) {

		if (node == parent->rb_right)
			parent->rb_right = left;
		else
			parent->rb_left = left;

	} else {

		head->rb_node = left;
	}

	adx_rbtree_set_parent(node, left);
}

void adx_rbtree_insert_color(adx_rbtree_node *node, adx_rbtree_head *head)
{

	adx_rbtree_node *parent, *gparent;

	while ((parent = rb_parent(node)) && rb_is_red(parent)) {

		gparent = rb_parent(parent);

		if (parent == gparent->rb_left) {

			register adx_rbtree_node *uncle = gparent->rb_right;
			if (uncle && rb_is_red(uncle)) {

				rb_set_black(uncle);
				rb_set_black(parent);
				rb_set_red(gparent);
				node = gparent;
				continue;
			}

			if (parent->rb_right == node) {

				register adx_rbtree_node *tmp;
				adx_rbtree_rotate_left(parent, head);
				tmp = parent;
				parent = node;
				node = tmp;
			}

			rb_set_black(parent);
			rb_set_red(gparent);
			adx_rbtree_rotate_right(gparent, head);

		} else {

			register adx_rbtree_node *uncle = gparent->rb_left;
			if (uncle && rb_is_red(uncle)) {

				rb_set_black(uncle);
				rb_set_black(parent);
				rb_set_red(gparent);
				node = gparent;
				continue;
			}

			if (parent->rb_left == node) {

				register adx_rbtree_node *tmp;
				adx_rbtree_rotate_right(parent, head);
				tmp = parent;
				parent = node;
				node = tmp;
			}

			rb_set_black(parent);
			rb_set_red(gparent);
			adx_rbtree_rotate_left(gparent, head);
		}
	}

	rb_set_black(head->rb_node);
}

void adx_rbtree_delete_color(adx_rbtree_node *node, adx_rbtree_node *parent, adx_rbtree_head *head)
{

	adx_rbtree_node *other = NULL;

	while ((!node || rb_is_black(node)) && node != head->rb_node) {

		if (parent->rb_left == node) {

			other = parent->rb_right;

			if (rb_is_red(other)) {

				rb_set_black(other);
				rb_set_red(parent);
				adx_rbtree_rotate_left(parent, head);
				other = parent->rb_right;
			}

			if ((!other->rb_left || rb_is_black(other->rb_left)) &&
					(!other->rb_right || rb_is_black(other->rb_right))) {

				rb_set_red(other);
				node = parent;
				parent = rb_parent(node);

			} else {

				if (!other->rb_right || rb_is_black(other->rb_right)) {

					rb_set_black(other->rb_left);
					rb_set_red(other);
					adx_rbtree_rotate_right(other, head);
					other = parent->rb_right;
				}

				adx_rbtree_set_color(other, rb_color(parent));
				rb_set_black(parent);
				rb_set_black(other->rb_right);
				adx_rbtree_rotate_left(parent, head);
				node = head->rb_node;
				break;
			}

		} else {

			other = parent->rb_left;

			if (rb_is_red(other)) {

				rb_set_black(other);
				rb_set_red(parent);
				adx_rbtree_rotate_right(parent, head);
				other = parent->rb_left;
			}

			if ((!other->rb_left || rb_is_black(other->rb_left)) &&
					(!other->rb_right || rb_is_black(other->rb_right))) {

				rb_set_red(other);
				node = parent;
				parent = rb_parent(node);

			} else {

				if (!other->rb_left || rb_is_black(other->rb_left)) {

					rb_set_black(other->rb_right);
					rb_set_red(other);
					adx_rbtree_rotate_left(other, head);
					other = parent->rb_left;
				}

				adx_rbtree_set_color(other, rb_color(parent));
				rb_set_black(parent);
				rb_set_black(other->rb_left);
				adx_rbtree_rotate_right(parent, head);
				node = head->rb_node;
				break;
			}
		}
	}

	if (node)rb_set_black(node);
}

void adx_rbtree_delete(adx_rbtree_head *head, adx_rbtree_node *node)
{

	int color;
	adx_rbtree_node *child, *parent;

	if (!node->rb_left) {

		child = node->rb_right;

	} else if (!node->rb_right) {

		child = node->rb_left;

	} else {

		adx_rbtree_node *old = node, *left;

		node = node->rb_right;
		while ((left = node->rb_left) != NULL)
			node = left;

		if (rb_parent(old)) {

			if (rb_parent(old)->rb_left == old)
				rb_parent(old)->rb_left = node;
			else
				rb_parent(old)->rb_right = node;

		} else {

			head->rb_node = node;
		}

		child = node->rb_right;
		parent = rb_parent(node);
		color = rb_color(node);

		if (parent == old) {

			parent = node;

		} else {

			if (child)adx_rbtree_set_parent(child, parent);
			parent->rb_left = child;

			node->rb_right = old->rb_right;
			adx_rbtree_set_parent(old->rb_right, node);
		}

		node->rb_parent_color = old->rb_parent_color;
		node->rb_left = old->rb_left;
		adx_rbtree_set_parent(old->rb_left, node);

		if (color == 1)adx_rbtree_delete_color(child, parent, head);
		return;
	}

	parent = rb_parent(node);
	color = rb_color(node);

	if (child)adx_rbtree_set_parent(child, parent);

	if (parent) {

		if (parent->rb_left == node)
			parent->rb_left = child;
		else
			parent->rb_right = child;

	} else {

		head->rb_node = child;
	}

	if (color == 1)adx_rbtree_delete_color(child, parent, head);
}

/* string */
adx_rbtree_node *adx_rbtree_string_add(adx_rbtree_head *head, adx_rbtree_node *new_node)
{
	adx_rbtree_node *parent = NULL;
	adx_rbtree_node **p = &head->rb_node;
	adx_rbtree_node *node = NULL;

	if (new_node->string == NULL)
		return NULL;

	while (*p) {

		parent = *p;
		node = (adx_rbtree_node *)parent;
		if (node->string == NULL)
			return NULL;

		int retval = strcmp(new_node->string, node->string);
		if (retval < 0) {

			p = &(*p)->rb_left;

		} else if (retval > 0) {

			p = &(*p)->rb_right;

		} else {

			return NULL;
		}
	}

	adx_rbtree_link_node(new_node, parent, p);
	adx_rbtree_insert_color(new_node, head);
	return node;
}

adx_rbtree_node *adx_rbtree_string_find(adx_rbtree_head *head, const char *key)
{
	adx_rbtree_node *p = head->rb_node;
	adx_rbtree_node *node = NULL;

	if (key == NULL)
		return NULL;

	while (p) {

		node = (adx_rbtree_node *)p;
		if (node->string == NULL)
			return NULL;

		int retval = strcmp(key, node->string);
		if (retval < 0) {

			p = p->rb_left;

		} else if (retval > 0) {

			p = p->rb_right;

		} else {

			return node;
		}
	}

	return NULL;
}

/* number */
adx_rbtree_node *adx_rbtree_number_add(adx_rbtree_head *head, adx_rbtree_node *new_node)
{

	adx_rbtree_node *parent = NULL;
	adx_rbtree_node **p = &head->rb_node;
	adx_rbtree_node *node = NULL;

	while (*p) {

		parent = *p;
		node = (adx_rbtree_node *)parent;

		if (new_node->number < node->number) {

			p = &(*p)->rb_left;

		} else if (new_node->number > node->number) {

			p = &(*p)->rb_right;

		} else {

			return NULL;
		}
	}

	adx_rbtree_link_node(new_node, parent, p);
	adx_rbtree_insert_color(new_node, head);
	return node;
}

adx_rbtree_node *adx_rbtree_number_find(adx_rbtree_head *head, int key)
{

	adx_rbtree_node *p = head->rb_node;
	adx_rbtree_node *node = NULL;

	while (p) {

		node = (adx_rbtree_node *)p;

		if (key < node->number) {

			p = p->rb_left;

		} else if (key > node->number) {

			p = p->rb_right;

		} else {

			return node;
		}
	}

	return NULL;
}

void _adx_rbtree_print(adx_rbtree_node *p)
{
	// adx_rbtree_node *node = (adx_rbtree_node *)p;
	// fprintf(stdout, "[tree][%d][%s]\n", node->number, node->string);
	if (p->rb_left) _adx_rbtree_print(p->rb_left);
	if (p->rb_right)_adx_rbtree_print(p->rb_right);
}

void adx_rbtree_print(adx_rbtree_head *head)
{
	if (!head->rb_node) return;
	_adx_rbtree_print(head->rb_node);
}


