#!/bin/bash

# Função para gerar valores aleatórios
function valor_aleatorio() {
    echo $((RANDOM % 100 + 1))
}

# Função para gerar opção -p ou -u aleatória
function opcao_aleatoria() {
    random=$((RANDOM % 2))
    if [ $random -eq 0 ]; then
        echo "-p"
    else
        echo "-u"
    fi
}

# Função para rodar o comando com valores aleatórios
function rodar_comando() {
    valor=$(valor_aleatorio)
    opcao=$(opcao_aleatoria)
    if [ $i -eq 1 ]; then
        comando="./client execute $valor -u sleep 2"
        echo "Comando: $comando"
        eval $comando
    else
        programas=("sleep" "exemplo" "ls" "ls -l" "pwd" "date" "whoami" "hostname" "df" "uname -a" "du -h" "uptime" "man ls" "who" "w")

        if [ "$opcao" == "-p" ]; then
            # Se a opção for -p, criar uma pipeline de programas
            pipeline=""
            num_programas=$((RANDOM % 4 + 2))  # Entre 2 e 5 programas aleatoriamente
            for ((i = 0; i < num_programas; i++)); do
                programa=${programas[$RANDOM % ${#programas[@]}]}
                if [ "$programa" == "sleep" ]; then
                    argumento=$((RANDOM % 20 + 1))
                    pipeline+="$programa $argumento"
                else
                    pipeline+="$programa"
                fi
                if [ $i -lt $(($num_programas - 1)) ]; then
                    pipeline+=" | "
                fi
            done
            comando="./client execute $valor $opcao \"$pipeline\""
        else
            # Se a opção for -u, selecionar apenas um programa
            programa=${programas[$RANDOM % ${#programas[@]}]}
            if [ "$programa" == "sleep" ]; then
                argumento=$((RANDOM % 20 + 1))
                comando="./client execute $valor $opcao \"$programa $argumento\""
            else
                comando="./client execute $valor $opcao \"$programa\""
            fi
        fi

    echo "Comando: $comando"
    eval $comando
    
    fi
}

# Roda o comando 5 vezes com valores aleatórios
for i in {1..5}; do
    rodar_comando
done