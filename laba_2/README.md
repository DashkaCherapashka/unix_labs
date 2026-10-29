# UNIX Lab 02 — Конкурентный контейнер

Конкурентное управление файлами в разделяемом Docker-томе.

## Как работает

- Контейнер в бесконечном цикле:
  1. Под `flock` на служебном файле `.lock` ищет первое незанятое имя
     `001`, `002`, ... и атомарно создаёт файл (`set -C` + `>`).
  2. Записывает в файл свой UUID и локальный порядковый номер.
  3. Ждёт 1 секунду, удаляет файл, ждёт ещё 1 секунду.
- UUID генерируется один раз при старте контейнера.
- Порядковый номер — локальная переменная контейнера (1, 2, 3, ...).

## Сборка

docker build -t conc-worker .

## Запуск

docker volume create shared-vol

docker run -d --name w1 -v shared-vol:/data conc-worker

for i in $(seq 1 10); do
  docker run -d --name "w$i" -v shared-vol:/data conc-worker
done

## Остановка

docker rm -f $(docker ps -aq --filter ancestor=conc-worker)
