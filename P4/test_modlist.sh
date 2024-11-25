#!/bin/bash

# Ruta al archivo especial de dispositivo del módulo
MODLIST_PROC="/proc/modlist"

# Función para insertar elementos en la lista
add_elements() {
    for i in $(seq 1 5); do
        echo "add $i" > $MODLIST_PROC
        echo "Added $i to the list."
        sleep 0.3 # Simula retraso para concurrencia
    done
}

# Función para eliminar elementos de la lista
remove_elements() {
    for i in $(seq 3 5); do
        echo "remove $i" > $MODLIST_PROC
        echo "Removed $i from the list."
        sleep 0.5
    done
}

# Función para limpiar la lista
cleanup_list() {
    echo "cleanup" > $MODLIST_PROC
    echo "Cleaned up the list."
}

# Función para consultar el contenido de la lista
read_list() {
    echo "Reading the list..."
    cat $MODLIST_PROC
}

# Función para realizar pruebas de concurrencia
test_concurrent_operations() {
    echo "Starting concurrent operations..."
    add_elements &  # Agrega elementos en un proceso en segundo plano
    remove_elements & # Elimina elementos en otro proceso
    wait # Espera a que ambos terminen
    echo "Concurrent operations completed."
    read_list
}

# Menu interactivo para el usuario
echo "Choose an operation:"
echo "1. Add elements to the list"
echo "2. Remove elements from the list"
echo "3. Cleanup the list"
echo "4. Read the list"
echo "5. Test concurrent operations"
echo "6. Automated test sequence"
read -p "Enter your choice: " choice

case $choice in
1)
    add_elements
    ;;
2)
    remove_elements
    ;;
3)
    cleanup_list
    ;;
4)
    read_list
    ;;
5)
    test_concurrent_operations
    ;;
6)
    echo "Automated sequence: Adding, removing, cleaning, and testing concurrency..."
    add_elements
    read_list
    remove_elements
    read_list
    cleanup_list
    test_concurrent_operations
    ;;
*)
    echo "Invalid choice."
    ;;
esac
