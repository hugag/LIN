#!/bin/bash


PROC_CONS="/dev/prodcons"


productor() {
    for i in $(seq 1 10); do
        echo "Productor intentando insertar $i..."

        if echo "$i" > "$PROC_CONS" 2>/dev/null; then
            echo "Productor: Agregado $i al buffer"
        else
            echo "Productor: Fallo al agregar $i, buffer lleno"
        fi
        sleep 0.5
    done
}


consumidor() {
    for i in $(seq 1 10); do
        echo "Consumidor intentando extraer..."
        if value=$(cat "$PROC_CONS" 2>/dev/null); then
            echo "Consumidor: Extraído $value del buffer"
        else
            echo "Consumidor: Fallo al extraer, buffer vacío"
        fi
        sleep 1
    done
}

echo "Iniciando prueba de ProdCons..."
sleep 1

productor &
consumidor &

wait

echo "Prueba de ProdCons finalizada."
