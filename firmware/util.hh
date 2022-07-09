#pragma once

/**
 * Reallocate a container to exactly fit its contents
 */
template<typename C> void shrinkContainer(C &container) {
    if (container.size() != container.capacity()) {
        C tmp = container;
        swap(container, tmp);
    }
}