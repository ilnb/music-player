#ifndef _LIST_H
#define _LIST_H

typedef struct list_head {
  struct list_head *next, *prev;
} list_head;

#define LIST_HEAD_INIT(head) {&(head), &(head)}
#define LIST_POISON1 ((void *)0x100)
#define LIST_POISON2 ((void *)0x122)

#define list_entry(ptr, type, member) ((type *)((void *)(ptr) - offsetof(type, member)))

static inline void __list_add(list_head *new_node, list_head *prev, list_head *next) {
  new_node->next = next;
  new_node->prev = prev;
  prev->next = new_node;
  next->prev = new_node;
}

static inline void list_add_head(list_head *new_node, list_head *head) { __list_add(new_node, head, head->next); }

static inline void list_add_tail(list_head *new_node, list_head *head) { __list_add(new_node, head->prev, head); }

static inline void __list_del(list_head *prev, list_head *next) {
  prev->next = next;
  next->prev = prev;
}

static inline void list_del(list_head *entry) {
  __list_del(entry->prev, entry->next);
  entry->next = (list_head *)LIST_POISON1;
  entry->prev = (list_head *)LIST_POISON2;
}

static inline int list_is_first(const list_head *list, const list_head *head) { return list->prev == head; }

static inline int list_is_last(const list_head *list, const list_head *head) { return list->next == head; }

static inline int list_is_head(const list_head *list, const list_head *head) { return list == head; }

static inline int list_empty(const list_head *head) { return head->next == head; }

#define list_first_entry(ptr, type, member) list_entry((ptr)->next, type, member)

#define list_last_entry(ptr, type, member) list_entry((ptr)->prev, type, member)

#define list_next_entry(pos, member) list_entry((pos)->member.next, typeof(*(pos)), member)

#define list_prev_entry(pos, member) list_entry((pos)->member.prev, typeof(*(pos)), member)

#define list_entry_is_head(pos, head, member) (&(pos)->member == head)

#define list_for_each_entry(pos, head, member) for (pos = list_first_entry(head, typeof(*pos), member); !list_entry_is_head(pos, head, member); pos = list_next_entry(pos, member))

#define list_for_each_rev(pos, head, member) for (pos = list_last_entry(head, typeof(*pos), member); !list_entry_is_head(pos, head, member); pos = list_prev_entry(pos, member))

#define list_for_each_entry_safe(pos, n, head, member)                                                                                                                             \
  for (pos = list_first_entry(head, typeof(*pos), member), n = list_next_entry(pos, member); !list_entry_is_head(pos, head, member); p = n, n = list_next_entry(pos, member))

#endif /* _LIST_H */
