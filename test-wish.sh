#! /bin/bash
# Офіційна обгортка, шлях до run-tests.sh змінено під структуру цього репозиторію.
if ! [[ -x wish ]]; then
    echo "wish executable does not exist"
    exit 1
fi

./tester/run-tests.sh $*
