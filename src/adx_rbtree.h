
#ifndef	__ADX_RBTREE_H__
#define	__ADX_RBTREE_H__

#ifdef __cplusplus
extern "C" { 
#endif

#include <stdio.h>
#include <string.h>

	typedef struct adx_rbtree_node {

		unsigned long  rb_parent_color;
		struct adx_rbtree_node *rb_right;
		struct adx_rbtree_node *rb_left;

		union {
			int number;
			char *string;
		};

	} adx_rbtree_node ;

	typedef struct {
		adx_rbtree_node *rb_node;

	} adx_rbtree_head;

	extern void adx_rbtree_insert_color(adx_rbtree_node *node, adx_rbtree_head *head);
	extern void adx_rbtree_link_node(adx_rbtree_node * node, adx_rbtree_node *parent, adx_rbtree_node **rb_link);
	extern void adx_rbtree_delete(adx_rbtree_head *head, adx_rbtree_node *node);

	/* string */ 
	extern adx_rbtree_node *adx_rbtree_string_add(adx_rbtree_head *head, adx_rbtree_node *new_node);
	extern adx_rbtree_node *adx_rbtree_string_find(adx_rbtree_head *head, const char *key);

	/* number */
	extern adx_rbtree_node *adx_rbtree_number_add(adx_rbtree_head *head, adx_rbtree_node *new_node);
	extern adx_rbtree_node *adx_rbtree_number_find(adx_rbtree_head *head, int key);

#define RB_ROOT	(adx_rbtree_head) {0}
#define rb_is_red(r) (!rb_color(r))
#define rb_is_black(r) rb_color(r)
#define rb_set_red(r) do { (r)->rb_parent_color &= ~1; } while (0)
#define rb_set_black(r) do { (r)->rb_parent_color |= 1; } while (0)
#define rb_parent(r) ((adx_rbtree_node *)((r)->rb_parent_color & ~3))
#define rb_color(r) ((r)->rb_parent_color & 1)
#define rb_offsetof(type,member) (size_t)(&((type *)0)->member)
#define rb_entry(ptr, type, member) ({const typeof(((type *)0)->member) * __mptr = (ptr);(type *)((char *)__mptr - rb_offsetof(type, member));})

#ifdef __cplusplus
}
#endif

#endif



