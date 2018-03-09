
#ifndef __ADX_LIST_H__
#define	__ADX_LIST_H__

#ifdef __cplusplus
extern "C" { 
#endif

	typedef struct adx_list_t adx_list_t;
	struct adx_list_t {

		adx_list_t *next;
		adx_list_t *prev;
	};

	static inline void adx_list_init(adx_list_t *head)
	{
		head->next = head;
		head->prev = head;
	}

	static inline void _adx_list_add(adx_list_t *node, adx_list_t *prev, adx_list_t *next)
	{
		next->prev = node;
		node->next = next;
		node->prev = prev;
		prev->next = node;
	}

	static inline void adx_list_add(adx_list_t *head, adx_list_t *node)
	{
		_adx_list_add(node, head->prev, head);
	}

	static inline void adx_list_add_tail(adx_list_t *head, adx_list_t *node)
	{
		_adx_list_add(node, head, head->next);
	}

	static inline void _adx_list_del(adx_list_t *prev, adx_list_t *next)
	{
		next->prev = prev;
		prev->next = next;
	}

	static inline void adx_list_del(adx_list_t *node)
	{
		_adx_list_del(node->prev, node->next);
	}

	static inline int adx_list_empty(adx_list_t *head)
	{
		adx_list_t *next = head->next;
		return (next == head) && (next == head->prev);
	}

	extern void adx_list_sort(void *priv, adx_list_t *head, int size,
			int (*cmp)(void *priv, adx_list_t *a, adx_list_t *b));

	extern adx_list_t *adx_queue(adx_list_t *head);

#define adx_list_for_each(pos, head) for (pos = (head)->next; pos != (head); pos = pos->next)
#define adx_list_for_tail(pos, head) for (pos = (head)->prev; pos != (head); pos = pos->prev)
#define adx_list_offsetof(type,member) (size_t)(&((type *)0)->member)
#define adx_list_entry(ptr, type, member) ({const typeof(((type *)0)->member) * __mptr = (ptr);(type *)((char *)__mptr - adx_list_offsetof(type, member));})

#ifdef __cplusplus
}
#endif

#endif


