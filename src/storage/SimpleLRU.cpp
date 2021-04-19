#include "SimpleLRU.h"
#include <sstream>
#include <string>


#include <iostream>


namespace Afina {
namespace Backend {

bool SimpleLRU::_move_to_head(lru_node &node) {
    if (&node == _lru_head.get()) {
        return true;
    }
    if (&node == _lru_tail) { // tail != head (!)
        _lru_tail = _lru_tail->prev;
    }
    lru_node *prev_node = node.prev;
    
    std::cout << (prev_node->next == nullptr) << " " << prev_node << std::endl;
    
    std::unique_ptr<lru_node> tmp_holder(std::move(prev_node->next));
    
    std::cout << (prev_node->next == nullptr) << " " << prev_node << std::endl;
    std::cout /*<< (tmp_holder->next == nullptr)*/ << " " << tmp_holder.get() << std::endl;

    prev_node->next = std::move(tmp_holder->next);
    _lru_head->prev = &node;
    tmp_holder->next = std::move(_lru_head);
    _lru_head = std::move(tmp_holder);
    return true;
}

bool SimpleLRU::_pop_lru_node() {
    if (_lru_tail == nullptr) { // pop from empty list
        return false;
    }
    auto cur_pos = _lru_index.find(_lru_tail->key);
    if (cur_pos == _lru_index.end()) { // incorrect tail
        return false;
    }
    lru_node &cur_elem = cur_pos->second.get();
    _lru_index.erase(cur_pos);
    _cur_size -= cur_elem.key.size() + cur_elem.value.size();
    if (&cur_elem == _lru_head.get()) { // list is empty
        _lru_head.reset(); 
    } else {
        _lru_tail = cur_elem.prev;
        _lru_tail->next.reset();
    }
    return true;
}


bool SimpleLRU::_free_space(std::size_t required) {
    while (_max_size - _cur_size < required) {
        if (!_pop_lru_node()) {
            return false;
        }
    }
    return true;
}


bool SimpleLRU::_put_new_node(const std::string &key, const std::string &value) {
    std::size_t elem_size = key.size() + value.size();
    if (!_free_space(elem_size)) {
        return false;
    }
    std::unique_ptr<lru_node> tmp_holder(new lru_node{ key, value, nullptr, nullptr });
    tmp_holder->next = std::move(_lru_head);
    _lru_head = std::move(tmp_holder);
    if (_lru_head->next != nullptr) { // list was not empty
        _lru_head->next->prev = _lru_head.get();
    } else {
        _lru_tail = _lru_head.get();
    }
    _cur_size += key.size() + value.size();

    _lru_index.insert({std::cref(_lru_head->key), std::ref(*(_lru_head.get()))});
    return true;
}


bool SimpleLRU::_set_val_node(lru_node &node, const std::string &new_value) {
    if (node.key.size() + new_value.size() > _max_size) {
        return false;
    }
    if (!_move_to_head(node)) { // so our node can not be popped out from list tail
        return false;
    }
    if (new_value.size() > node.value.size()) {
        if (!_free_space(new_value.size() - node.value.size())) {
            return false;
        }
    }
    _cur_size += new_value.size() - node.value.size();
    node.value = new_value;
    return true;
}


bool SimpleLRU::_erase_storage_node(lru_node &node) {
    if (!_move_to_head(node)) {
        return false;
    }
    _lru_head = std::move(node.next);
    if (_lru_head->next == nullptr) { // case single node
        _lru_tail = nullptr;
    }
    return true;
}

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::Put(const std::string &key, const std::string &value) { 
    Out();
    if (key.size() + value.size() > _max_size) {
        return false;
    }
    auto cur_pos = _lru_index.find(key);
    if (cur_pos == _lru_index.end()) {
        return _put_new_node(key, value);
    }
    lru_node &node = cur_pos->second.get();
    return _set_val_node(node, value);
}

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::PutIfAbsent(const std::string &key, const std::string &value) {
    Out();
    if (key.size() + value.size() > _max_size) {
        return false;
    }
    auto cur_pos = _lru_index.find(key);
    if (cur_pos != _lru_index.end()) {
        return false;
    }
    return _put_new_node(key, value);
}

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::Set(const std::string &key, const std::string &value) {
    Out();
    if (key.size() + value.size() > _max_size) {
        return false;
    }
    auto cur_pos = _lru_index.find(key);    
    if (cur_pos == _lru_index.end()) {
        return false;
    }
    return _set_val_node(cur_pos->second.get(), value);
}

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::Delete(const std::string &key) {
    Out();
    auto cur_pos = _lru_index.find(key);
    if (cur_pos == _lru_index.end()) {
        return false;
    }
    lru_node &cur_node = cur_pos->second.get();
    _lru_index.erase(cur_pos);
    _cur_size -= cur_node.key.size() + cur_node.value.size();
    return _erase_storage_node(cur_node);
}

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::Get(const std::string &key, std::string &value) {
    Out();
    auto cur_pos = _lru_index.find(key);
    if (cur_pos == _lru_index.end()) {
        return false;
    }
    lru_node &cur_node = cur_pos->second.get();
    value = cur_node.value;
    return _move_to_head(cur_node);
}

void SimpleLRU::Out() {
    for (lru_node *tmp = _lru_head.get(); tmp != nullptr; tmp = tmp->next.get()) {
        std::cout << tmp->key << ": " << tmp->value << std::endl;
    }
}


} // namespace Backend
} // namespace Afina
