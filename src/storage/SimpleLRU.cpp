#include "SimpleLRU.h"
#include <sstream>
#include <string>


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
    std::unique_ptr<lru_node> tmp_holder = std::move(prev_node->next);
    prev_node->next = std::move(tmp_holder->next);
    _lru_head->prev = &node;
    tmp_holder->next =std::move(_lru_head);
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

bool SimpleLRU::_put_new_node(const std::string &key, const std::string &value) {
    std::size_t elem_size = key.size() + value.size();
    if (elem_size > _max_size) {
        return false;
    } 
    while (elem_size + _cur_size > _max_size) {
        if (!_pop_lru_node()) {
            return false;
        }
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
    return true;
}


bool SimpleLRU::_set_val_node(lru_node &node, const std::string &new_value) {
    std::size_t new_elem_size = node.key.size() + new_value.size();
    if (new_elem_size > _max_size) {
        return false;
    }
    if (!_move_to_head(node)) { // so our node can not be popped out from list tail
        return false;
    }
    while (_cur_size - node.value.size() + new_value.size() > _max_size) {
        if(!_pop_lru_node()) {
            return false;
        }
    }
    _cur_size += new_value.size();
    _cur_size -= node.value.size();
    node.value = new_value;
    return true;
}


bool SimpleLRU::_erase_storage_node(lru_node &node) {
    if (!_move_to_head(node)) { // put node to head
        return false;
    }
    _lru_head = std::move(node.next); // erase head
    return true;
}

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::Put(const std::string &key, const std::string &value) { 
    auto cur_pos = _lru_index.find(key);
    if (cur_pos == _lru_index.end()) {
        if (_put_new_node(key, value)) {
            _lru_index.insert({std::reference_wrapper<const std::string>(_lru_head->key), 
                               std::reference_wrapper<lru_node>(*(_lru_head.get()))});
            return true;
        }
    } else {
        lru_node &node = cur_pos->second.get();
        if (_set_val_node(node, value)) {
            _lru_index.at(std::reference_wrapper<const std::string>(_lru_head->key)) = 
                    std::reference_wrapper<lru_node>(*(_lru_head.get()));
            return true;
        }
    }
    return false;
}

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::PutIfAbsent(const std::string &key, const std::string &value) {
    auto cur_pos = _lru_index.find(key);
    if (cur_pos != _lru_index.end()) {
        return false;
    }
    if (_put_new_node(key, value)) {
        _lru_index.insert({std::reference_wrapper<const std::string>(_lru_head->key), 
                           std::reference_wrapper<lru_node>(*(_lru_head.get()))});
        return true;
    } else {
        return false;
    }
}

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::Set(const std::string &key, const std::string &value) {
    auto cur_pos = _lru_index.find(key);
    if (cur_pos == _lru_index.end()) {
        return false;
    }
    return _set_val_node(cur_pos->second.get(), value);
}

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::Delete(const std::string &key) {
    auto cur_pos = _lru_index.find(key);
    if (cur_pos == _lru_index.end()) {
        return false;
    }
    lru_node &cur_node = cur_pos->second.get();
    _lru_index.erase(cur_pos);
     return _erase_storage_node(cur_node);
}

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::Get(const std::string &key, std::string &value) {
    auto cur_pos = _lru_index.find(key);
    if (cur_pos == _lru_index.end()) {
        return false;
    }
    lru_node &cur_node = cur_pos->second.get();
    value = cur_node.value;
    return _move_to_head(cur_node);
}

} // namespace Backend
} // namespace Afina
