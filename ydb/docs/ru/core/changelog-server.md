# Список изменений {{ ydb-short-name }} Server

## Версия 25.1 {#25-1}

### Релиз кандидат 25.1.2.7 {#25-1-2-7-rc}

Дата выхода: 14 июля 2025.


#### Функциональность

* [Реализован](https://github.com/ydb-platform/ydb/pull/19504) [векторный индекс](./dev/vector-indexes.md) для приближённого векторного поиска. Для векторного поиска опубликованы рецепты для [YDB CLI и YQL](https://ydb.tech/docs/ru/recipes/vector-search/), а также примеры работы [на С++ и Python](https://ydb.tech/docs/ru/recipes/ydb-sdk/vector-search). Включается установкой флага `enable_vector_index` в [конфигурации кластера](./reference/configuration/index.md). Внимание! После включения флага откат на предыдущие версии {{ ydb-short-name }} невозможен.
* [Добавлена](https://github.com/ydb-platform/ydb/issues/11454) поддержка {% if feature_async_replication %}[консистентной асинхронной репликации](./concepts/async-replication.md){% else %}консистентной асинхронной репликации{% endif %}.
* Поддержаны запросы [BATCH UPDATE](.yql/reference/syntax/batch-update) и [BATCH DELETE](./yql/reference/syntax/batch-delete), позволяющие изменять большие строковые таблицы вне транзакционных ограничений. Включается установкой флага `enable_batch_updates` в конфигурации кластера.
* Добавлен [механизм конфигурации V2](./devops/configuration-management/configuration-v2/config-overview), упрощающий развёртывание новых кластеров {{ ydb-short-name }} и дальнейшую работу с ними. [Сравнение](./devops/configuration-management/compare-configs) механизмов конфигурации V1 и V2.
* Добавлена поддержка параметризованного [типа Decimal](./yql/reference/types/primitive.md#numeric).
* Реализована клиентская балансировка партиций при чтении по [протоколу Kafka](https://kafka.apache.org/documentation/#consumerconfigs_partition.assignment.strategy) (как у самой Kafka). Раньше балансировка происходила на сервере. Включается установкой флага `enable_kafka_native_balancing` в конфигурации кластера.
* Добавлена поддержка [автопартиционирования топиков](./concepts/cdc.md#topic-partitions) в CDC для строковых таблиц. Включается установкой флага `enable_topic_autopartitioning_for_cdc` в конфигурации кластера.
* [Добавлена](https://github.com/ydb-platform/ydb/pull/8264) возможность [изменить время хранения данных](./concepts/cdc.md#topic-options) в CDC-топике с использованием выражения `ALTER TOPIC`.
* [Поддержан](https://github.com/ydb-platform/ydb/pull/7052) [формат DEBEZIUM_JSON](./concepts/cdc.md#debezium-json-record-structure) для потоков изменений (changefeed).
* [Добавлена](https://github.com/ydb-platform/ydb/pull/19507) возможность создавать потоки изменений к индексным таблицам.
* Добавлена возможность [указания числа реплик](./yql/reference/syntax/alter_table/indexes.md) для вторичного индекса. Включается установкой флага `enable_access_to_index_impl_tables` в конфигурации кластера.
* В операциях резервного копирования и восстановления расширен состав поддерживаемых объектов. Включается установкой флагов, указанных в скобках:
  * [поддержка](https://github.com/ydb-platform/ydb/issues/7054) потока изменений (флаги `enable_changefeeds_export` и `enable_changefeeds_import`);
  * [поддержка](https://github.com/ydb-platform/ydb/issues/12724) представлений (`VIEW`) (флаг `enable_view_export`).
* Добавлено автоудаление временных директорий и таблиц при экспорте в S3. Включается установкой флага `enable_export_auto_dropping` в конфигурации кластера.
* [Добавлена](https://github.com/ydb-platform/ydb/pull/12909) автоматическая проверка целостности резервных копий при импорте, предотвращающая восстановление из повреждённых резервных копий и защищающая от потери данных.
* [Добавлена](https://github.com/ydb-platform/ydb/pull/15570) возможность создания представлений, использующих [UDF](./yql/reference/builtins/basic.md#udf) в запросах.
* Добавлены системные представления с информацией о [настройках прав доступа](./dev/system-views#auth), [истории перегруженных партиций](./dev/system-views#top-overload-partitions) - включается установкой флага `enable_followers_stats` в конфигурации кластера,  [истории партиций строковых таблиц со сломанными блокировками (TLI)](./dev/system-views#top-tli-partitions).
* Добавлены новые параметры в операторы [CREATE USER](./yql/reference/syntax/create-user.md) и [ALTER USER](./yql/reference/syntax/alter-user.md):
  * `HASH` — возможность задания пароля в зашифрованном виде;
  * `LOGIN` и `NOLOGIN` — разблокировка и блокировка пользователя.
* Повышена безопасность учётных записей:
  * [Добавлена](https://github.com/ydb-platform/ydb/pull/11963) [проверка сложности пароля](./reference/configuration/auth_config#password-complexity) пользователя;
  * [Реализована](https://github.com/ydb-platform/ydb/pull/12578) [автоматическая блокировка пользователя](./reference/configuration/auth_config#account-lockout) при исчерпании лимита попыток ввода пароля;
  * [Добавлена](https://github.com/ydb-platform/ydb/pull/12983) возможность самостоятельной смены пароля пользователем.
* [Реализована](https://github.com/ydb-platform/ydb/issues/9748) возможность переключения функциональных флагов во время работы сервера {{ ydb-short-name }}. Флаги, для которых в [proto-файле](https://github.com/ydb-platform/ydb/blob/main/ydb/core/protos/feature_flags.proto#L60) не указан параметр `(RequireRestart) = true`, будут применяться без рестарта кластера.
* Теперь самые старые (а не новые) блокировки [меняются на полношардовые](https://github.com/ydb-platform/ydb/pull/11329) при превышении количества блокировок на шардах.
* [Реализовано](https://github.com/ydb-platform/ydb/pull/12567) сохранение оптимистичных блокировок в памяти при плавном перезапуске даташардов, что должно уменьшить число ошибок ABORTED из-за потери блокировок при балансировке таблиц между узлами.
* [Реализована](https://github.com/ydb-platform/ydb/pull/12689) отмена волатильных транзакций со статусом ABORTED при плавном перезапуске даташардов.
* [Добавлена](https://github.com/ydb-platform/ydb/pull/6342) возможность удалить `NOT NULL`-ограничения на столбец в таблице с помощью запроса `ALTER TABLE ... ALTER COLUMN ... DROP NOT NULL`.
* [Добавлено](https://github.com/ydb-platform/ydb/pull/9168) ограничение в 100 000 на число одновременных запросов на создание сессий в сервисе координации.
* [Увеличено](https://github.com/ydb-platform/ydb/pull/14219) максимальное [число столбцов в первичном ключе](./concepts/limits-ydb.md?#schema-object) с 20 до 30.
* Улучшена диагностика и интроспекция ошибок, связанных с памятью ([#10419](https://github.com/ydb-platform/ydb/pull/10419), [#11968](https://github.com/ydb-platform/ydb/pull/11968)).
* **_(Экспериментально)_** [Добавлен](https://github.com/ydb-platform/ydb/pull/14075) экспериментальный режим работы с более строгими проверками прав доступа. Включается установкой следующих флагов:
  * `enable_strict_acl_check` – не позволять выдавать права несуществующим пользователям и удалять пользователей, если им выданы права;
  * `enable_strict_user_management` — включает строгие правила администрирования локальных пользователей (т.е. администрировать локальных пользователей может только администратор кластера или базы данных);
  * `enable_database_admin` — добавляет роль администратора базы данных.

#### Изменения с потерей обратной совместимости

* Если вы используете запросы, в которых происходит обращение к именованным выражениям как к таблицам с помощью [AS_TABLE](./yql/reference/syntax/select/from_as_table), обновите [temporal over YDB](https://github.com/yandex/temporal-over-ydb) на версию [v1.23.0-ydb-compat](https://github.com/yandex/temporal-over-ydb/releases/tag/v1.23.0-ydb-compat) перед обновление YDB на текущую версию, чтобы избежать ошибок в выполнении таких запросов.

#### YDB UI

* В редактор запросов [добавлена](https://github.com/ydb-platform/ydb-embedded-ui/pull/1974) поддержка частичной загрузки результатов — отображение начинается сразу при получении первого фрагмента с сервера без ожидания полного завершения запроса. Это позволяет быстрее получать результаты.
* [Улучшена](https://github.com/ydb-platform/ydb-embedded-ui/pull/1967) безопасность: элементы управления, которые недоступны пользователю, теперь не отображаются в интерфейсе. Пользователи не будут сталкиваться с ошибками "Доступ запрещен".
* [Добавлен](https://github.com/ydb-platform/ydb-embedded-ui/pull/1981) добавлен поиск по идентификатору таблетки на вкладку "Tablets".
* Добавлена подсказка по горячим клавишам, которая открывается по комбинации `⌘+K`.
* На страницу базы данных добавлена вкладка "Операции", которая позволяет просматривать список операций и отменять их.
* Обновлена панель мониторинга кластера, добавлена возможность ее свернуть.
* Реализована поддержка поиска с учетом регистра в инструменте иерархического отображения JSON.
* На верхнюю панель после выбора базы данных добавлены примеры кода для подключения в YDB SDK, что ускоряет процесс разработки.
* Исправлена сортировка строк на вкладке Запросы.
* Удалены лишние запросы подтверждения при закрытии страницы браузера в редакторе запросов — подтверждение запрашивается только когда это необходимо.

#### Производительность

* [Добавлена](https://github.com/ydb-platform/ydb/pull/6509) поддержка [свёртки констант](https://ru.wikipedia.org/wiki/%D0%A1%D0%B2%D1%91%D1%80%D1%82%D0%BA%D0%B0_%D0%BA%D0%BE%D0%BD%D1%81%D1%82%D0%B0%D0%BD%D1%82) в оптимизаторе запросов по умолчанию, что повышает производительность запросов за счёт вычисления константных выражений на этапе компиляции.
* [Добавлен](https://github.com/ydb-platform/ydb/issues/6512) новый протокол гранулярного таймкаста, который позволит сократить время выполнения распределённых транзакций (замедление одного шарда не будет приводить к замедлению всех).
* [Реализована](https://github.com/ydb-platform/ydb/issues/11561) функциональность сохранения состояния даташардов в памяти при перезапусках, что позволяет сохранить блокировки и повысить шансы на успешное выполнение транзакций. Это сокращает время выполнения длительных транзакций за счёт уменьшения числа повторных попыток.
* [Реализована](https://github.com/ydb-platform/ydb/pull/15255) конвейерная обработка внутренних транзакций в [Node Broker](./concepts/glossary.md#node-broker), что ускорило запуск динамических узлов в кластере {{ ydb-short-name }}.
* [Улучшена](https://github.com/ydb-platform/ydb/pull/15607) устойчивость Node Broker к повышенной нагрузке со стороны узлов кластера.
* [Включены](https://github.com/ydb-platform/ydb/pull/19440) по умолчанию выгружаемые B-Tree-индексы вместо невыгружаемых SST-индексов, что позволяет сократить потребление памяти при хранении «холодных» данных.
* [Оптимизировано](https://github.com/ydb-platform/ydb/pull/15264) потребление памяти узлами хранения.
* [Сократили](https://github.com/ydb-platform/ydb/pull/10969) время запуска Hive до 30%.
* [Оптимизирован](https://github.com/ydb-platform/ydb/pull/6561) процесс репликации в распределённом хранилище.
* [Оптимизирован](https://github.com/ydb-platform/ydb/pull/9491) размер заголовка больших двоичных объектов в VDisk.
* [Уменьшено](https://github.com/ydb-platform/ydb/pull/15517) потребление памяти за счёт очистки страниц аллокатора.

#### Исправления ошибок

* [Исправлена](https://github.com/ydb-platform/ydb/pull/9707) ошибка в настройке [Interconnect](./concepts/glossary.md#actor-system-interconnect), приводящая к снижению производительности.
* [Исправлена](https://github.com/ydb-platform/ydb/pull/13993) ошибка «Out of memory» при удалении очень больших таблиц за счёт регулирования числа одновременно обрабатывающих данную операцию таблеток.
* [Исправлена](https://github.com/ydb-platform/ydb/pull/9848) ошибка, возникавшая при указании одного и того же узла базы данных несколько раз в конфигурации для системных таблеток.
* [Устранена](https://github.com/ydb-platform/ydb/pull/11059) ошибка длительного (секунды) чтения данных при частых операциях перешардирования таблицы.
* [Исправлена](https://github.com/ydb-platform/ydb/pull/9723) ошибка чтения из асинхронных реплик, приводившая к сбою.
* [Исправлены](https://github.com/ydb-platform/ydb/pull/9507) редкие зависания при первоначальном сканировании [CDC](./dev/cdc.md).
* [Исправлена](https://github.com/ydb-platform/ydb/pull/11483) обработка незавершённых схемных транзакций в даташардах при перезапуске системы.
* [Исправлена](https://github.com/ydb-platform/ydb/pull/10460) ошибка несогласованного чтения из топика при попытке явно подтвердить сообщение, прочитанное в рамках транзакции. Теперь пользователь при попытке подтвердить сообщение получит ошибку.
* [Исправлена](https://github.com/ydb-platform/ydb/pull/12220) ошибка, из-за которой автопартиционирование некорректно работало при работе с топиком в транзакции.
* [Исправлены](https://github.com/ydb-platform/ydb/pull/12905) зависания транзакций при работе с топиками во время перезапуска таблеток.
* [Исправлена](https://github.com/ydb-platform/ydb/pull/13910) ошибка «Key is out of range» при импорте из S3-совместимого хранилища.
* [Исправлено](https://github.com/ydb-platform/ydb/pull/13741) некорректное определение конца поля с метаданными в конфигурации кластера.
* [Улучшено](https://github.com/ydb-platform/ydb/pull/16420) построение вторичных индексов: при возникновении некоторых ошибок система ретраит процесс, а не прерывает его.
* [Исправлена](https://github.com/ydb-platform/ydb/pull/16635) ошибка выполнения выражения `RETURNING` в запросах `INSERT INTO` и `UPSERT INTO`.
* [Исправлена](https://github.com/ydb-platform/ydb/pull/16269) проблема зависания операции «Drop Tablet» в PQ tablet, особенно во время задержек в работе Interconnect.
* [Исправлена](https://github.com/ydb-platform/ydb/pull/16194) ошибка, возникавшая во время [компакшна](./concepts/glossary.md#compaction) VDisk.
* [Исправлена](https://github.com/ydb-platform/ydb/pull/15233) проблема, из-за которой длительные сессии чтения топика завершались с ошибками «too big inflight».
* [Исправлено](https://github.com/ydb-platform/ydb/pull/15515) зависание при чтении топика, если хотя бы одна партиция не имела входящих данных, но читалась несколькими потребителями.
* [Устранена](https://github.com/ydb-platform/ydb/pull/18614) редкая проблема перезагрузок PQ tablet.
* [Устранена](https://github.com/ydb-platform/ydb/pull/18378) проблема, при которой после обновления версии кластера Hive запускались подписчики в датацентрах без работающих узлов баз данных.
* [Исправлена](https://github.com/ydb-platform/ydb/pull/19057) ошибка `Failed to set up listener on port 9092 errno# 98 (Address already in use)`, возникавшая при обновлении версии.
* [Исправлена](https://github.com/ydb-platform/ydb/pull/18905) ошибка, приводившая к segmentation fault при одновременном выполнении запроса к healthcheck и отключении узла кластера.
* [Исправлен](https://github.com/ydb-platform/ydb/pull/18899) сбой в [партиционировании строковой таблицы](./concepts/datamodel/table.md#partitioning_row_table) при выборе разделённого ключа из образцов доступа, содержащих смешанные операции с полным ключом и префиксом ключа (например, точное чтение или чтение диапазона).
* [Исправлена](https://github.com/ydb-platform/ydb/pull/18647) [ошибка](https://github.com/ydb-platform/ydb/issues/17885), из-за которой тип индекса ошибочно определялся как `GLOBAL SYNC`, хотя в запросе явно указывался `UNIQUE`.
* [Исправлена](https://github.com/ydb-platform/ydb/pull/16797) ошибка, из-за которой автопартиционирование топиков не работало, когда параметр конфигурации `max_active_partition` задавался с помощью выражения `ALTER TOPIC`.
* [Исправлена](https://github.com/ydb-platform/ydb/pull/18938) ошибка, из-за которой `ydb scheme describe` возвращал список столбцов не в том порядке, в котором они были заданы при создании таблицы.

## Версия 24.4 {#24-4}

### Версия 24.4.4.12 {#24-4-4-12}

Дата выхода: 3 июня 2025.

#### Производительность

* [Ограничено](https://github.com/ydb-platform/ydb/pull/17755) количество одновременно обрабатываемых изменений конфигураций.
* [Оптимизировано](https://github.com/ydb-platform/ydb/issues/18289) потребление памяти PQ-таблетками.
* [Оптимизировано](https://github.com/ydb-platform/ydb/issues/18473) потребление CPU таблеткой Scheme shard, что уменьшило задержки ответов на запросы. Теперь лимит на число операций Scheme shard  проверяется до выполнения операций разделения и слияния таблеток.

#### Исправления ошибок

* [Исправлена](https://github.com/ydb-platform/ydb/pull/17123) редкая ошибка зависания клиентских приложений во время выполнения коммита транзакции, когда удаление партиции совершалось раньше обновления квоты на запись в топик.
* [Исправлена](https://github.com/ydb-platform/ydb/pull/17312) ошибка в копировании таблиц с типом Decimal, которая приводила к сбою при откате на предыдущую версию.
* [Исправлена](https://github.com/ydb-platform/ydb/pull/17519) [ошибка](https://github.com/ydb-platform/ydb/issues/17499), при которой коммит без подтверждения записи в топик приводил к блокировке текущей и следующих транзакций с топиками.
* Исправлены зависания транзакций при работе с топиками при [перезагрузке](https://github.com/ydb-platform/ydb/issues/17843) или [удалении](https://github.com/ydb-platform/ydb/issues/17915) таблетки.
* [Исправлены](https://github.com/ydb-platform/ydb/pull/18114) [проблемы](https://github.com/ydb-platform/ydb/issues/18071) с чтением сообщений больше 6Mb через [Kafka API](./reference/kafka-api).
* [Устранена](https://github.com/ydb-platform/ydb/pull/18319) утечка памяти во время записи в [топик](./concepts/glossary#topic).
* Исправлены ошибки обработки [nullable столбцов](https://github.com/ydb-platform/ydb/issues/15701) и [столбцов с типом UUID]((https://github.com/ydb-platform/ydb/issues/15697)) в строковых таблицах.

### Версия 24.4.4.2 {#24-4-4-2}

Дата выхода: 15 апреля 2025.

#### Функциональность

* Включены по умолчанию:

  * поддержка {% if feature_view %}[представлений (VIEW)](./concepts/datamodel/view.md){% else %}представлений (VIEW){% endif %};
  * режим [автопартиционирования](./concepts/topic.md#autopartitioning) топиков;
  * [транзакции с участием топиков и строковых таблиц](./concepts/transactions.md#topic-table-transactions);
  * [волатильные распределённые транзакции](./contributor/datashard-distributed-txs.md#osobennosti-vypolneniya-volatilnyh-tranzakcij).

* Добавлена возможность [чтения и записи в топик](./reference/kafka-api/examples.md#primery-raboty-s-kafka-api) с использованием Kafka API без аутентификации.

#### Производительность

* Включен по умолчанию [автоматический выбор вторичного индекса](./dev/secondary-indexes.md#avtomaticheskoe-ispolzovanie-indeksov-pri-vyborke) при выполнении запроса.

#### Исправления ошибок

* [Исправлена](https://github.com/ydb-platform/ydb/pull/14811) ошибка, которая приводила к существенному снижению скорости чтения с [подписчиков таблетки](./concepts/glossary.md#tablet-follower).
* [Исправлена](https://github.com/ydb-platform/ydb/pull/14516) ошибка, которая приводила к ожиданию подтверждения волатильной распределённой транзакции до следующего рестарта.
* [Исправлена](https://github.com/ydb-platform/ydb/pull/15077) редкая ошибка, которая приводила к сбою при подключении подписчиков таблетки к лидеру с несогласованным состоянием журнала команд.
* [Исправлена](https://github.com/ydb-platform/ydb/pull/15074) редкая ошибка, которая приводила к сбою при перезапуске удалённого datashard с неконсистентными изменениями.
* [Исправлена](https://github.com/ydb-platform/ydb/pull/15194) ошибка, из-за которой мог нарушаться порядок обработки сообщений в топике.
* [Исправлена](https://github.com/ydb-platform/ydb/pull/15308) редкая ошибка, из-за которой могло зависать чтение из топика.
* [Исправлена](https://github.com/ydb-platform/ydb/pull/15160) проблема, из-за которой транзакция зависала при одновременном управлении топиком пользователем и перемещении PQ таблетки на другой узел.
* [Исправлена](https://github.com/ydb-platform/ydb/pull/15233) проблема с утечкой значения счётчика для userInfo, которая могла приводить к ошибке чтения `too big in flight`.
* [Исправлен](https://github.com/ydb-platform/ydb/pull/15467) сбой прокси-сервера из-за дублирования топиков в запросе.
* [Исправлена](https://github.com/ydb-platform/ydb/pull/15933) редкая ошибка, из-за которой пользователь мог писать в топик в обход ограничений квоты аккаунта.
* [Исправлена](https://github.com/ydb-platform/ydb/pull/16288) проблема, из-за которой после удаления топика система возвращала "OK", но его таблетки продолжали работать. Для удаления таких таблеток воспользуйтесь инструкцией из [pull request](https://github.com/ydb-platform/ydb/pull/16288).
* [Исправлена](https://github.com/ydb-platform/ydb/pull/16418) редкая ошибка, из-за которой не восстанавливалась резервная копия большой таблицы с вторичным индексом.
* [Исправлена](https://github.com/ydb-platform/ydb/pull/15862) проблема, которая приводила к ошибке при вставке данных с помощью `UPSERT` в строковые таблицы с значениями по умолчанию.
* [Устранена](https://github.com/ydb-platform/ydb/pull/15334) ошибка, которая приводила к сбою при выполнении запросов к таблицам с вторичными индексами, возвращающих списки результатов с помощью выражения `RETURNING *`.

## Версия 24.3 {#24-3}

### Версия 24.3.15.5 {#24-3-15-5}

Дата выхода: 6 февраля 2025.

#### Функциональность

* Добавлена возможность регистрировать [узел базы данных](./concepts/glossary.md#database-node) по сертификату. В [Node Broker](./concepts/glossary.md#node-broker) добавлен флаг `AuthorizeByCertificate` использования сертификата при регистрации.
* [Добавлены](https://github.com/ydb-platform/ydb/pull/11775) приоритеты проверки аутентификационных тикетов [с использованием стороннего IAM-провайдера](./security/authentication.md#iam), с самым высоким приоритетом обрабатываются запросы от новых пользователей. Тикеты в кеше обновляют свою информацию с приоритетом ниже.

#### Производительность

* [Ускорено](https://github.com/ydb-platform/ydb/pull/12747) поднятие таблеток на больших кластерах:​ 210 мс **→** 125 мс (ssd)​, 260 мс **→** 165 мс (hdd)​.

#### Исправления ошибок

* [Снято](https://github.com/ydb-platform/ydb/pull/11901) ограничение на запись в тип Uint8 значений больше 127.
* [Исправлена](https://github.com/ydb-platform/ydb/pull/12221) ошибка, из-за которой при чтении из топика маленьких сообщений маленькими порциями значительно увеличивалась назгрузка на CPU. Это могло приводить к задержкам в чтении/записи в этот топик.
* [Исправлена](https://github.com/ydb-platform/ydb/pull/12915) ошибка восстановления из резервной копии, сохраненной в хранилище S3 с Path-style адресацией.
* [Исправлена](https://github.com/ydb-platform/ydb/pull/13918) ошибка восстановления из резервной копии, которая была создана в момент автоматического разделения таблицы.
* [Исправлена](https://github.com/ydb-platform/ydb/pull/12601) ошибка в сериализации `Uuid` для [CDC](./concepts/cdc.md).
* [Исправлена](https://github.com/ydb-platform/ydb/pull/12018) потенциальная поломка ["замороженных" блокировок](./contributor/datashard-locks-and-change-visibility#vzaimodejstvie-s-raspredelyonnymi-tranzakciyami), к которой могли приводить массовые операции (например, удаление по TTL).
* [Исправлена](https://github.com/ydb-platform/ydb/pull/12804) ​​ошибка, из-за которой чтение на подписчиках таблетки могло приводить к сбоям во время автоматического разделения таблицы.
* [Исправлена](https://github.com/ydb-platform/ydb/pull/12807) ошибка, при которой [узел координации](./concepts/datamodel/coordination-node.md) успешно регистрировал прокси-серверы несмотря на разрыв связи.
* [Исправлена](https://github.com/ydb-platform/ydb/pull/11593) ошибка, возникающие при открытии в интерфейсе вкладки с информацией о [группах распределенного хранилища](./concepts/glossary.md#storage-group).
* [Исправлена](https://github.com/ydb-platform/ydb/pull/12448) [ошибка](https://github.com/ydb-platform/ydb/issues/12443), из-за которой [Health Check](./reference/ydb-sdk/health-check-api) не сообщал о проблемах в синхронизации времени.
* [Исправлена](https://github.com/ydb-platform/ydb/pull/11658) редкая проблема, которая приводила к ошибкам при выполнении запроса на чтение.
* [Исправлена](https://github.com/ydb-platform/ydb/pull/13501) редкая проблема, которая приводила к утечкам незакоммиченных изменений.
* [Исправлены](https://github.com/ydb-platform/ydb/pull/13948) проблемы с согласованностью, связанные с кэшированием удаленных диапазонов.

### Версия 24.3.11.14 {#24-3-11-14}

Дата выхода: 9 января 2025.

* [Поддержан](https://github.com/ydb-platform/ydb/pull/11276) рестарт без потери доступности кластера в [минимальной отказоустойчивой конфигурации](./concepts/topology#reduced) из трех узлов.
* [Добавлены](https://github.com/ydb-platform/ydb/pull/13218) новый функции UDF Roaring bitmap: AndNotWithBinary, FromUint32List, RunOptimize

### Версия 24.3.11.13 {#24-3-11-13}


Дата выхода: 24 декабря 2024.

#### Функциональность

* Добавлена [трассировка запросов](./reference/observability/tracing/setup) – инструмент, позволяющий детально посмотреть путь следования запроса по распределенной системе.
* Добавлена поддержка [асинхронной репликации](./concepts/async-replication), которая позволяет синхронизировать данные между базами YDB почти в реальном времени. Также она может быть использована для миграции данных между базами с минимальным простоем работающих с ними приложений.
* Добавлена поддержка [представлений (VIEW)](https://ydb.tech/docs/ru/concepts/datamodel/view), которая может быть включена администратором кластера с помощью настройки `enable_views` в [динамической конфигурации](./maintenance/manual/dynamic-config#obnovlenie-dinamicheskoj-konfiguracii).
* В [федеративных запросах](./concepts/federated_query/) поддержаны новые внешние источники данных: MySQL, Microsoft SQL Server, Greenplum.
* Разработана [документация](./devops/deployment-options/manual/federated-queries/connector-deployment) по разворачиванию YDB с функциональностью федеративных запросов (в ручном режиме).
* Для Docker-контейнера с YDB добавлен параметр запуска `FQ_CONNECTOR_ENDPOINT`, позволяющий указать адрес коннектора ко внешним источникам данных. Добавлена возможность TLS-шифрования соединения с коннектором. Добавлена возможность вывода порта сервиса коннектора, локально работающего на том же хосте, что и динамический узел YDB.
* Добавлен режим [автопартиционирования](./concepts/topic#autopartitioning) топиков, в котором топики могут разбивать партиции в зависимости от нагрузки с сохранением гарантий порядка чтения сообщений и exactly once записи. Режим может быть включен администратором кластера с помощью настроек `enable_topic_split_merge` и `enable_pqconfig_transactions_at_scheme_shard` в [динамической конфигурации](./maintenance/manual/dynamic-config#obnovlenie-dinamicheskoj-konfiguracii).
* Добавлены [транзакции](./concepts/transactions#topic-table-transactions) с участием [топиков](https://ydb.tech/docs/ru/concepts/topic) и строковых таблиц. Таким образом, можно транзакционно перекладывать данные из таблиц в топики и в обратном направлении, а также между топиками, чтобы данные не терялись и не дублировались. Транзакции могут быть включены администратором кластера с помощью настроек `enable_topic_service_tx` и `enable_pqconfig_transactions_at_scheme_shard` в [динамической конфигурации](./maintenance/manual/dynamic-config#obnovlenie-dinamicheskoj-konfiguracii).
* [Добавлена](https://github.com/ydb-platform/ydb/pull/7150) поддержка [CDC](./concepts/cdc) для синхронных вторичных индексов.
* Добавлена возможность изменить период хранения записей в [CDC]((./concepts/cdc)) топиках.
* Добавлена поддержка [автоинкремента](./yql/reference/types/serial) для колонок, включенных в первичный ключ таблицы.
* Добавлена запись в [аудитный лог](./security/audit-log) событий логина пользователей в YDB, событий завершения сессии пользователя в пользовательском интерфейсе, а также запроса бэкапа и восстановления из бэкапа.
* Добавлено системное представление, позволяющее получить информацию о сессиях, установленных с базой данных, с помощью запроса.
* Добавлена поддержка константных значений по умолчанию для колонок строковых таблиц.
* Добавлена поддержка выражения `RETURNING` в запросах.
* Добавлена [встроенная функция](./yql/reference/builtins/basic.md#version) `version()`.
* [Добавлены](https://github.com/ydb-platform/ydb/pull/8708) время запуска/завершения и автор в метаданные операций резервного копирования/восстановления из S3-совместимого хранилища.
* Добавлена поддержка резервного копирования/восстановления из S3-совместимого хранилища ACL для таблиц.
* Для запросов, читающих из S3, в план добавлены пути и метод декомпрессии.
* Добавлены новые настройки парсинга для `timestamp`, `datetime` при чтении данных из S3.
* Добавлена поддержка типа `Decimal` в [ключах партиционирования](https://ydb.tech/docs/ru/dev/primary-key/column-oriented#klyuch-particionirovaniya).
* Улучшена диагностика проблем хранилища в HealthCheck.
* **_(Экспериментально)_** Добавлен [стоимостной оптимизатор](./concepts/optimizer#stoimostnoj-optimizator-zaprosov) для сложных запросов, где участвуют [колоночные таблицы](./concepts/glossary#column-oriented-table). Оптимизатор рассматривает большое количество альтернативных планов выполнения и выбирает из них лучший на основе оценки стоимости каждого варианта. На текущий момент оптимизатор работает только с планами, где есть операции [JOIN](./yql/reference/syntax/join).
* **_(Экспериментально)_** Реализована начальная версия [менеджера рабочей нагрузки](./dev/resource-consumption-management), который позволяет создавать пулы ресурсов с ограничениями по процессору, памяти и количеству активных запросов. Реализованы классификаторы ресурсов для отнесения запросов к определенному пулу ресурсов.
* **_(Экспериментально)_** Реализован [автоматический выбор индекса](https://ydb.tech/docs/ru/dev/secondary-indexes#avtomaticheskoe-ispolzovanie-indeksov-pri-vyborke) при выполнении запроса, который может быть включен администратором кластера с помощью настройки `index_auto_choose_mode` в `table_service_config` в [динамической конфигурации](./maintenance/manual/dynamic-config#obnovlenie-dinamicheskoj-konfiguracii).

#### YDB UI

* Поддержано создание и [отображение](https://github.com/ydb-platform/ydb-embedded-ui/issues/782) экземпляра асинхронной репликации.
* [Добавлено](https://github.com/ydb-platform/ydb-embedded-ui/issues/929) обозначение [столбцов с автоинкрементом](./yql/reference/types/serial).
* [Добавлена](https://github.com/ydb-platform/ydb-embedded-ui/pull/1438) вкладка с информацией о [таблетках](./concepts/glossary#tablet).
* [Добавлена](https://github.com/ydb-platform/ydb-embedded-ui/pull/1289) вкладка с информацией о [группах распределенного хранилища](./concepts/glossary#storage-group).
* [Добавлена](https://github.com/ydb-platform/ydb-embedded-ui/pull/1218) настройка для добавления [трассировки](./reference/observability/tracing/setup) ко всем запросам и отображение результатов трассировки запроса.
* На страницу PDisk добавлены [атрибуты](https://github.com/ydb-platform/ydb-embedded-ui/pull/1069), информация о потреблении дискового пространства, а также кнопка, которая запускает [декомиссию диска](./devops/deployment-options/manual/decommissioning).
* [Добавлена](https://github.com/ydb-platform/ydb-embedded-ui/pull/1313) информация о выполняющихся запросах.
* [Добавлена](https://github.com/ydb-platform/ydb-embedded-ui/pull/1291) настройка лимита строк в выдаче для редактора запроса и отображение, если результаты запроса превысили лимит.
* [Добавлено](https://github.com/ydb-platform/ydb-embedded-ui/pull/1049) отображение перечня запросов с максимальным потреблением CPU за последний час.
* [Добавлен](https://github.com/ydb-platform/ydb-embedded-ui/pull/1127) поиск на страницах с историей запросов и списком сохраненных запросов.
* [Добавлена](https://github.com/ydb-platform/ydb-embedded-ui/pull/1117) возможность прервать исполнение запроса.
* [Добавлена](https://github.com/ydb-platform/ydb-embedded-ui/issues/944) возможность сохранять запрос из редактора горячими клавишами.
* [Разделено](https://github.com/ydb-platform/ydb-embedded-ui/pull/1422) отображение дисков от дисков-доноров.
* [Добавлена](https://github.com/ydb-platform/ydb-embedded-ui/pull/1154) поддержка InterruptInheritance ACL и улучшено отображение действующих ACL.
* [Добавлено](https://github.com/ydb-platform/ydb-embedded-ui/pull/889) отображение текущей версии пользовательского интерфейса.
* [Добавлена](https://github.com/ydb-platform/ydb-embedded-ui/pull/1229) с информацией о состоянии настроек включения экспериментальной функциональности.

#### Производительность

* [Ускорено](https://github.com/ydb-platform/ydb/pull/7589) восстановление из бэкапа таблиц со вторичными индексами до 20% по нашим тестам.
* [Оптимизирована](https://github.com/ydb-platform/ydb/pull/9721) пропускная способность Interconnect.
* Улучшена производительность CDC-топиков, содержащих тысячи партиций.
* Сделан ряд улучшений алгоритма балансировки таблеток Hive.

#### Исправления ошибок

* [Исправлена](https://github.com/ydb-platform/ydb/pull/6850) ошибка, которая приводила в неработоспособное состояние базу с большим количеством таблиц или партиций при восстановлении из резервной копии. Теперь при превышении лимитов на размер базы, операция восстановления завершится ошибкой, база продолжит работать в штатном режиме.
* [Реализован](https://github.com/ydb-platform/ydb/pull/11532) механизм, принудительно запускающий фоновый [компакшн](./concepts/glossary#compaction) при обнаружении несоответствий между схемой данных и данными, хранящимися в [DataShard](./concepts/glossary#data-shard). Это решает редко возникающую проблему задержки в изменении схемы данных.
* [Устранено](https://github.com/ydb-platform/ydb/pull/10447) дублирование аутентификационных тикетов, которое приводило к повышенному числу запросов в провайдеры аутентификации.
* [Исправлена](https://github.com/ydb-platform/ydb/pull/9377) ошибка нарушения инварианта при первоначальном сканировании CDC, приводившая к аварийному завершению серверного процесса ydbd.
* [Запрещено](https://github.com/ydb-platform/ydb/pull/9446) изменение схемы таблиц резервного копирования.
* [Исправлено](https://github.com/ydb-platform/ydb/pull/9509) зависание первоначального сканирования CDC при частых обновлениях таблицы.
* [Исключены](https://github.com/ydb-platform/ydb/pull/9934) удаленные индексы из подсчета лимита на [максимальное количество индексов](https://ydb.tech/docs/ru/concepts/limits-ydb#schema-object).
* [Исправлена](https://github.com/ydb-platform/ydb/pull/8847) [ошибка](https://github.com/ydb-platform/ydb/issues/6985) в отображении времени, на которое запланировано выполнение набора транзакций (планируемый шаг).
* [Исправлена](https://github.com/ydb-platform/ydb/pull/9161) [проблема](https://github.com/ydb-platform/ydb/issues/8942) прерывания blue–green deployment в больших кластерах, возникающая из-за частого обновления списка узлов.
* [Исправлена](https://github.com/ydb-platform/ydb/pull/8925) редко возникающая ошибка, которая приводила к нарушению порядка выполнения транзакций.
* [Исправлена](https://github.com/ydb-platform/ydb/pull/9841) [ошибка](https://github.com/ydb-platform/ydb/issues/9797) в EvWrite API, которая приводила к некорректному освобождению памяти.
* [Исправлена](https://github.com/ydb-platform/ydb/pull/10698) [проблема](https://github.com/ydb-platform/ydb/issues/10674) зависания волатильных транзакций после перезапуска.
* Исправлена ошибка в CDC, приводящая в некоторых случаях к повышенному потреблению CPU, вплоть до ядра на одну CDC-партицию.
* [Устранена](https://github.com/ydb-platform/ydb/pull/11061) задержка чтения, возникающая во время и после разделения некоторых партиций.
* Исправлены ошибки при чтении данных из S3.
* [Исправлен](https://github.com/ydb-platform/ydb/pull/4793) способ расчета aws signature при обращении к S3.
* Исправлены ложные срабатывания системы HealthCheck в момент бэкапа базы с большим количеством шардов.

## Версия 24.2 {#24-2}

Дата выхода: 20 августа 2024.

### Функциональность

* Добавлена возможность [задать приоритеты](./devops/deployment-options/manual/maintenance-without-downtime#priority) задачам обслуживания в [системе управления кластером](./concepts/glossary#cms).
* Добавлена [настройка стабильных имён](reference/configuration/index.md#node-broker-config) для узлов кластера в рамках тенанта.
* Добавлено получение вложенных групп от [LDAP-сервера](./security/authentication.md#ldap), в [LDAP-конфигурации](reference/configuration/index.md#ldap-auth-config) улучшен парсинг хостов и добавлена настройка для отключения встроенной аутентификацию по логину и паролю.
* Добавлена возможность аутентификации [динамических узлов](./concepts/glossary#dynamic) по SSL-сертификату.
* Реализовано удаление неактивных узлов из [Hive](./concepts/glossary#hive) без его перезапуска.
* Улучшено управление inflight pings при перезапуске Hive в кластерах большого размера.
* [Изменен](https://github.com/ydb-platform/ydb/pull/6381) порядок установления соединения с узлами при перезапуске Hive.

### YDB UI

* [Добавлена](https://github.com/ydb-platform/ydb/pull/7485) возможность задать TTL для сессии пользователя в конфигурационном файле.
* [Добавлена](https://github.com/ydb-platform/ydb-embedded-ui/pull/1028) сортировка по `CPUTime` в таблицу со списком запросов.
* [Исправлена](https://github.com/ydb-platform/ydb/pull/7779) потеря точности при работе с `double`, `float`.
* Поддержано [создание директорий из UI](https://github.com/ydb-platform/ydb-embedded-ui/issues/958).
* [Добавлена возможность](https://github.com/ydb-platform/ydb-embedded-ui/pull/976) задать интервал фонового обновления данных на всех страницах.
* [Улучшено](https://github.com/ydb-platform/ydb-embedded-ui/issues/955) отображения ACL.
* Включено автодополнение в редакторе запросов по умолчанию.
* [Добавлена](https://github.com/ydb-platform/ydb-embedded-ui/pull/834) поддержка View.

### Исправления ошибок

* Добавлена проверка на размер локальной транзакции до ее коммита, чтобы исправить [ошибки](https://github.com/ydb-platform/ydb/issues/6677) в работе схемных операции при выполнении экспорта/бекапа больших баз.
* [Исправлена](https://github.com/ydb-platform/ydb/pull/7709) [ошибка](https://github.com/ydb-platform/ydb/issues/7674) дублирования результатов SELECT-запроса при уменьшении квоты в [DataShard](./concepts/glossary#data-shard).
* [Исправлены](https://github.com/ydb-platform/ydb/pull/6461) [ошибки](https://github.com/ydb-platform/ydb/issues/6220), возникающие при изменении состояния [координатора](./concepts/glossary#coordinator).
* [Исправлены](https://github.com/ydb-platform/ydb/pull/5992) ошибки, возникающие в момент первичного сканирования [CDC](./dev/cdc).
* [Исправлено](https://github.com/ydb-platform/ydb/pull/6615) состояние гонки в асинхронной доставке изменений (асинхронные индексы, CDC).
* [Исправлена](https://github.com/ydb-platform/ydb/pull/5993) редкая ошибка, из-за которой удаление по [TTL](./concepts/ttl) приводило к аварийному завершению процесса.
* [Исправлена](https://github.com/ydb-platform/ydb/pull/5760) ошибка отображения статуса PDisk в интерфейсе [CMS](./concepts/glossary#cms).
* [Исправлены](https://github.com/ydb-platform/ydb/pull/6008) ошибки, из-за которых мягкий перенос (drain) таблеток с узла мог зависать.
* [Исправлена](https://github.com/ydb-platform/ydb/pull/6445) ошибка остановки interconnect proxy на узле, работающем без перезапусков, при добавлении другого узла в кластер.
* [Исправлен](https://github.com/ydb-platform/ydb/pull/6695) учет свободной памяти в [interconnect](./concepts/glossary#actor-system-interconnect).
* [Исправлены](https://github.com/ydb-platform/ydb/issues/6405) счетчики UnreplicatedPhantoms/UnreplicatedNonPhantoms в VDisk.
* [Исправлена](https://github.com/ydb-platform/ydb/issues/6398) обработка пустых запросов сборки мусора на VDisk.
* [Исправлено](https://github.com/ydb-platform/ydb/pull/5894) управление настройками TVDiskControls через CMS.
* [Исправлена](https://github.com/ydb-platform/ydb/pull/5883) ошибка загрузки данных, созданных более новыми версиями VDisk.
* [Исправлена](https://github.com/ydb-platform/ydb/pull/5862) ошибка выполнении запроса `REPLACE INTO` со значением по умолчанию.
* [Исправлена](https://github.com/ydb-platform/ydb/pull/7714) ошибка исполнения запросов, в которых выполнялось несколько left join'ов к одной строковой таблице.
* [Исправлена](https://github.com/ydb-platform/ydb/pull/7740) потеря точности для `float`, `double` типов при использовании CDC.


## Версия 24.1 {#24-1}

Дата выхода: 31 июля 2024.

### Функциональность

* Реализована [Knn UDF](./yql/reference/udf/list/knn.md) для точного поиска ближайших векторов.
* Разработан gRPC сервис QueryService, обеспечивающий возможность выполнения всех типов запросов (DML, DDL) и выборку неограниченных объёмов данных.
* Реализована [интеграция с LDAP протоколом](./security/authentication.md) и возможность получения перечня групп из внешних LDAP-каталогов.

### Встроенный UI

* Добавлен дашборд диагностики потребления ресурсов, который находится на вкладке с информацией о базе данных и позволяет определить текущее состояние потребление основных ресурсов: ядер процессора, оперативной памяти и места в сетевом распределенном хранилище.
* Добавлены графики для мониторинга основных показателей работы кластера {{ ydb-short-name }}.

### Производительность

* [Оптимизированы](https://github.com/ydb-platform/ydb/pull/1837) таймауты сессий сервиса координации от сервера до клиента. Ранее таймаут составлял 5 секунд, что в худшем случае приводило к определению неработающего клиента (и освобождению удерживаемых им ресурсов) в течение 10 секунд. В новой версии время проверки зависит от времени ожидания сеанса, что обеспечивает более быстрое реагирование при смене лидера или захвате распределённых блокировок.
* [Оптимизировано](https://github.com/ydb-platform/ydb/pull/2391) потребление CPU репликами [SchemeShard](./concepts/glossary.md#scheme-shard), особенно при обработке быстрых обновлений для таблиц с большим количеством партиций.

### Исправления ошибок

* [Исправлена](https://github.com/ydb-platform/ydb/pull/3917) ошибка возможного переполнения очереди, [Change Data Capture](./dev/cdc.md) резервирует емкость очереди изменений при первоначальном сканировании.
* [Исправлена](https://github.com/ydb-platform/ydb/pull/4597) потенциальная взаимоблокировка между получением записей CDC и их отправкой.
* [Исправлена](https://github.com/ydb-platform/ydb/pull/2056) проблема потери очереди задач медиатора при переподключении медиатора, исправление позволяет обработать очередь задач медиатора при ресинхронизации.
* [Исправлена](https://github.com/ydb-platform/ydb/pull/2624) редко возникающая ошибка, когда при включённых и используемых волатильных транзакциях возвращался успешный результат подтверждения транзакции до того, как она была успешно закоммичена. Волатильные транзакции по умолчанию выключены, находятся в разработке.
* [Исправлена](https://github.com/ydb-platform/ydb/pull/2839) редко возникающая ошибка, приводившая к потере установленных блокировок и успешному подтверждению транзакций, которые должны были завершиться ошибкой Transaction Locks Invalidated.
* [Исправлена](https://github.com/ydb-platform/ydb/pull/3074) редкая ошибка, приводящая к возможному нарушению гарантий целостности данных при конкурентной записи и чтении данных по определённому ключу.
* [Исправлена](https://github.com/ydb-platform/ydb/pull/4343) проблема, из-за которой реплики для чтения переставали обрабатывать запросы.
* [Исправлена](https://github.com/ydb-platform/ydb/pull/4979) редкая ошибка, которая могла привести к аварийному завершению процессов базы данных при наличии неподтверждённых транзакций над таблицей в момент её переименования.
* [Исправлена](https://github.com/ydb-platform/ydb/pull/3632) ошибка в логике определения статуса статической группы, когда статическая группа не помечалась нерабочей, хотя должна была.
* [Исправлена](https://github.com/ydb-platform/ydb/pull/2169) ошибка частичного коммита распределённой транзакции с незакоммиченными изменениями в случае некоторых гонок с рестартами.
* [Исправлены](https://github.com/ydb-platform/ydb/pull/2374) аномалии с чтением устаревших данных, которые были [обнаружены с помощью Jepsen](https://blog.ydb.tech/hardening-ydb-with-jepsen-lessons-learned-e3238a7ef4f2).


## Версия 23.4 {#23-4}

Дата выхода: 14 мая 2024.

### Производительность

* [Исправлена](https://github.com/ydb-platform/ydb/pull/3638) проблема повышенного потребления вычислительных ресурсов актором топиков `PERSQUEUE_PARTITION_ACTOR`.
* [Оптимизировано](https://github.com/ydb-platform/ydb/pull/2083) использование ресурсов репликами SchemeBoard. Наибольший эффект заметен при модификации метаданных таблиц с большим количеством партиций.

### Исправления ошибок

* [Исправлена](https://github.com/ydb-platform/ydb/pull/2169) ошибка возможной неполной фиксации накопленных изменений при использовании распределенных транзакций. Данная ошибка возникает при крайне редкой комбинации событий, включающей в себя перезапуск таблеток, обслуживающих вовлеченные в транзакцию партиции таблиц.
* [Устранена](https://github.com/ydb-platform/ydb/pull/3165) гонка между процессами слияния таблиц и сборки мусора, из-за которой сборка мусора могла завершиться ошибкой нарушения инвариантов и, как следствие, аварийным завершением серверного процесса `ydbd`.
* [Исправлена](https://github.com/ydb-platform/ydb/pull/2696) ошибка в Blob Storage, из-за которой информация о смене состава группы хранения могла не поступать своевременно на отдельные узлы кластера. В результате в редких случаях могли блокироваться операции чтения и записи данных, хранящихся в затронутой группе, и требовалось ручное вмешательство администратора.
* [Исправлена](https://github.com/ydb-platform/ydb/pull/3002) ошибка в Blob Storage, из-за которой при корректной конфигурации могли не запускаться узлы хранения данных. Ошибка проявлялась для систем с явным образом включенной экспериментальной функцией "blob depot" (по умолчанию эта функция выключена).
* [Исправлена](https://github.com/ydb-platform/ydb/pull/2475) ошибка, возникавшая в некоторых ситуациях записи в топик с пустым `producer_id` при выключенной дедупликации. Она могла приводить к аварийному завершению серверного процесса `ydbd`.
* [Исправлена](https://github.com/ydb-platform/ydb/pull/2651) проблема, приводящая к падению процесса `ydbd` из-за ошибочного состояния сессии записи в топик.
* [Исправлена](https://github.com/ydb-platform/ydb/pull/3587) ошибка отображения метрики количества партиций в топике, ранее в ней отображалось некорректное значение.
* [Устранены](https://github.com/ydb-platform/ydb/pull/2126) утечки памяти, которые проявлялись при копировании данных топиков между кластерами {{ ydb-short-name }}. Они могли приводить к завершению серверных процессов `ydbd` из-за исчерпания доступной оперативной памяти.


## Версия 23.3 {#23-3}

Дата выхода: 12 октября 2023.

### Функциональность

* Реализована видимость собственных изменений внутри транзакций. Ранее при попытке прочитать данные, уже модифицированные в текущей транзакцией, запрос завершался ошибкой. Это приводило к необходимости упорядочивать чтения и записи внутри транзакции. С появлением видимости собственных изменений эти ограничения снимаются, и запросы могут читать измененные в данной транзакции строчки.
* Добавлена поддержка [колоночных таблиц](concepts/datamodel/table.md#column-tables). Колоночные таблицы хорошо подходят для работы с аналитическими запросами (Online Analytical Processing), так как при выполнении запроса считываются только те столбцы, которые непосредственно участвуют в запросе. Колоночные таблицы YDB позволяют создавать аналитические отчёты с производительностью, сопоставимой со специализированными аналитическими СУБД.
* Добавлена поддержка [Kafka API для топиков](reference/kafka-api/index.md). Теперь с YDB-топиками можно работать через Kafka-совместимый API, предназначенный для миграции существующих приложений. Обеспечена поддержка протокола Kafka версии 3.4.0.
* Добавлена возможность [записи в топик без дедупликации](concepts/topic.md#no-dedup). Такой вид записи хорошо подходит для случаев, когда порядок обработки сообщений не критичен. Запись без дедупликации работает быстрее и потребляет меньше ресурсов на сервере, но упорядочение и дедупликация сообщений на сервере не происходит.
* В YQL добавлены возможности [создавать](yql/reference/syntax/create-topic.md), [изменять](yql/reference/syntax/alter-topic.md) и [удалять](yql/reference/syntax/drop-topic.md) топики.
* Добавлена возможность назначать и отзывать права доступа с помощью команд YQL [GRANT](yql/reference/syntax/grant.md) и [REVOKE](yql/reference/syntax/revoke.md).
* Добавлена возможность логгировать DML-операции в аудитном логе.
* **_(Экспериментально)_** При записи сообщений в топик теперь можно передавать метаданные. Для включения этой функциональности добавьте `enable_topic_message_meta: true` в [конфигурационный файл](reference/configuration/index.md).
* **_(Экспериментально)_** Добавлена возможность [чтения из топиков](reference/ydb-sdk/topic.md#read-tx) и запись в таблицу в рамках одной транзакции. Новая возможность упрощает сценарий переноса данных из топика в таблицу. Для её включения добавьте `enable_topic_service_tx: true` в конфигурационный файл.
* **_(Экспериментально)_** Добавлена поддержка [совместимости с PostgreSQL](postgresql/intro.md). Новый механизм позволяет выполнять SQL запросы в PostgreSQL диалекте на инфраструктуре YDB с использованием сетевого протокола PostgreSQL. Можно использовать привычные инструменты работы с PostgreSQL, такие, как psql и драйверы (pq для Golang и psycopg2 для Python), а также разрабатывать запросы на привычном PostgreSQL синтаксисе с горизонтальной масштабируемостью и отказоустойчивость YDB.
* **_(Экспериментально)_** Добавлена поддержка [федеративных запросов](concepts/federated_query/index.md). Она позволяет получать информацию из различных источников данных без их переноса в YDB. Поддерживается взаимодействие с ClickHouse, PostgreSQL, S3 через YQL-запросы без дублирования данных между системами.

### Встроенный UI

* В настройках селектора типа запроса добавлена новая опция `PostgreSQL`, которая доступна при включении параметра `Enable additional query modes`. Также в истории запросов теперь учитывается синтаксис, используемый при выполнении запроса.
* Обновлен шаблон YQL-запроса для создания таблицы. Добавлено описание доступных параметров.
* Сортировка и фильтрация для таблиц Storage и Nodes вынесена на сервер. Необходимо включить параметр `Offload tables filters and sorting to backend` в разделе экспериментов, чтобы использовать данный функционал.
* В контекстное меню были добавлены кнопки для создания, изменения и удаления [топиков](concepts/topic.md).
* Добавлена сортировка по критичности для всех issues в дереве в `Healthcheck`.

### Производительность

* Реализованы итераторные чтения. Новая функциональность позволяет разделить чтения и вычисления между собой. Итераторные чтения позволяют даташардам увеличить пропускную способность читающих запросов.
* Оптимизирована производительность записи в топики YDB.
* Улучшена балансировка таблеток при перегрузке нод.

### Исправления ошибок

* Исправлена ошибка возможной блокировки читающими итераторами снепшотов, о которых не знают координаторы.
* Исправлена утечка памяти при закрытии соединения в kafka proxy.
* Исправлена ошибка, при которой снепшоты, взятые через читающие итераторы, могут не восстанавливаться на рестартах.
* Исправлен некорректный residual предикат для условия `IS NULL` на колонку.
* Исправлена срабатывающая проверка `VERIFY failed: SendResult(): requirement ChunksLimiter.Take(sendBytes) failed`.
* Исправлен `ALTER TABLE` по `TTL` для колоночных таблиц.
* Реализован `FeatureFlag`, который позволяет отключать/включать работу с `CS` и `DS`.
* Исправлено различие координаторного времени между 23-2 и 23-3 на 50мс.
* Исправлена ошибка, при которой ручка `storage` возвращала лишние группы, когда в запросе параметр `node_id` во `viewer backend`.
* Добавлен `usage` фильтр в `/storage` во `viewer backend`.
* Исправлена ошибка в Storage v2, при которой возвращалось некорректное число в `Degraded`.
* Исправлена отмена подписки от сессий в итераторных чтениях при рестарте таблетки.
* Исправлена ошибка, при которой во время роллинг-рестарта при походе через балансер моргает `healthcheck` алертами про storage.
* Обновлены метрики `cpu usage` в ydb.
* Исправлено игнорирование `NULL` при указании `NOT NULL` в схеме таблицы.
* Реализован вывод записей об операциях `DDL` в общий лог.
* Реализован запрет для команды `ydb table attribute add/drop` работать с любыми объектами, кроме таблиц.
* Отключён `CloseOnIdle` для `interconnect`.
* Исправлено задваивание скорости чтения в UI.
* Исправлена ошибка, при которой могли теряться данные на `block-4-2`.
* Добавлена проверка имени топика.
* Исправлен возможный `deadlock` в акторной системе.
* Исправлен тест `KqpScanArrowInChanels::AllTypesColumns`.
* Исправлен тест `KqpScan::SqlInParameter`.
* Исправлены проблемы параллелизма для OLAP-запросов.
* Исправлена вставка `ClickBench parquet`.
* Добавлен недостающий вызов `CheckChangesQueueOverflow` в общем `CheckDataTxReject`.
* Исправлена ошибка возврата пустого статуса при вызовах `ReadRows API`.
* Исправлен некорректны ретрай экспорта в финальной стадии.
* Исправлена проблема с бесконечной квотой на число записей в CDC-топике.
* Исправлена ошибка импорта колонки `string` и `parquet` в колонку `string` OLAP.
* Исправлено падение `KqpOlapTypes.Timestamp` под tsan.
* Исправлено падение во `viewer backend` при попытке выполнить запрос к базе из-за несовместимости версий.
* Исправлена ошибка, при которой `viewer` не возвращал ответ от `healthcheck` из-за таймаута.
* Исправлена ошибка, при которой в Pdisk'ах могло сохраняться некорректное значение `ExpectedSerial`.
* Исправлена ошибка, при которой ноды базы падают по `segfault` в S3 акторе.
* Исправлена гонка в `ThreadSanitizer: data race KqpService::ToDictCache-UseCache`.
* Исправлена гонка в `GetNextReadId`.
* Исправлено завышение результата `SELECT COUNT(*)` сразу после импорта.
* Исправлена ошибка, при которой `TEvScan` мог вернуть пустой набор данных в случае сплита даташарда.
* Добавлен отдельный issue/код ошибки в случае исчерпания доступного места.
* Исправлена ошибка `GRPC_LIBRARY Assertion failed`.
* Исправлена ошибка, при которой при чтении по вторичному индексу в сканирующих запросах получался пустой результат.
* Исправлена валидация `CommitOffset` в `TopicAPI`.
* Уменьшено потребление `shared cache` при приближении к OOM.
* Смержена логика планировщиков из `data executer` и `scan executer` в один класс.
* Добавлены ручки `discovery` и `proxy` в процесс выполнения `query` во `viewer backend`.
* Исправлена ошибка, при которой ручка `/cluster` возвращает название корневого домена типа `/ru` во `viewer backend`.
* Реализована схема бесшовного обновления табличек для `QueryService`.
* Исправлена ошибка, при которой `DELETE` возвращал данные и НЕ удалял их.
* Исправлена ошибка работы `DELETE ON` в `query service`.
* Исправлено неожиданное выключение батчинга в дефолтных настройках схемы.
* Исправлена срабатывающая проверка `VERIFY failed: MoveUserTable(): requirement move.ReMapIndexesSize() == newTableInfo->Indexes.size()`.
* Увеличен дефолтный таймаут grpc-сриминга.
* Исключены неиспользуемые сообщения и методы из `QueryService`.
* Добавлена сортировка по `Rack` в `/nodes` во `viewer backend`.
* Исправлена ошибка, при которой запрос с сортировкой возвращает ошибку при убывании.
* Исправлено взаимодействие `KQP` с `NodeWhiteboard`.
* Удалена поддержка старых форматов параметров.
* Исправлена ошибка, при которой `DefineBox` не применялся для дисков, на которых есть статическая группа.
* Исправлена ошибка `SIGSEGV` в диннодах при импорте `CSV` через `YDB CLI`.
* Исправлена ошибка с падением при обработке `NGRpcService::TRefreshTokenImpl`.
* Реализован `gossip` протокол обмена информацией о ресурсах кластера.
* Исправлена ошибка `DeserializeValuePickleV1(): requirement data.GetTransportVersion() == (ui32) NDqProto::DATA_TRANSPORT_UV_PICKLE_1_0 failed`.
* Реализованы автоинкрементные колонки.
* Использовать статус `UNAVAILABLE` вместо `GENERIC_ERROR` при ошибке идентификации шарда.
* Добавлена поддержка `rope payload` в `TEvVGet`.
* Добавлено игнорирование устаревших событий.
* Исправлено падение write-сессий на невалидном имени топика.
* Исправлена ошибка `CheckExpected(): requirement newConstr failed, message: Rewrite error, missing Distinct((id)) constraint in node FlatMap`.
* Включён `safe heal` по умолчанию.

## Версия 23.2 {#23-2}

Дата выхода: 14 августа 2023.

### Функциональность

* **_(Экспериментально)_** Реализована видимость собственных изменений. При включении этой функции вы можете читать измененные значения из текущей транзакции, которая еще не была закоммичена. Также эта функциональность позволяет выполнять несколько модифицирующих операций в одной транзакции над таблицей с вторичными индексами. Для включения этой функциональности добавьте `enable_kqp_immediate_effects: true` в секцию `table_service_config` в [конфигурационный файл](reference/configuration/index.md).
* **_(Экспериментально)_** Реализованы итераторные чтения. Эта функциональность позволяет разделить чтения и вычисления между собой. Итераторные чтения позволяют даташардам увеличить пропускную способность читающих запросов. Для включения этой функциональности добавьте `enable_kqp_data_query_source_read: true` в секцию `table_service_config` в [конфигурационный файл](reference/configuration/index.md).

### Встроенный UI

* Улучшена навигация:
  * Кнопки переключения между режимами диагностики и разработки вынесены на левую панель.
  * На всех страницах добавлены хлебные крошки.
  * На странице базы данных информация о группах хранения и узлах базы перенесена во вкладки.
* История и сохраненные запросы перенесены во вкладки над редактором запросов.
* На вкладках Info для объектов схемы настройки выведены в терминах конструкции `CREATE` или `ALTER`.
* Поддержано отображение [колоночных таблиц](concepts/datamodel/table.md#column-table) в дереве схемы.

### Производительность

* Для сканирующих запросов реализована возможность эффективного поиска отдельных строк с использованием первичного ключа или вторичных индексов, что позволяет во многих случаях значительно улучшить производительность. Как и в обычных запросах, для использования вторичного индекса необходимо явно указать его имя в тексте запроса с использованием ключевого слова `VIEW`.

* **_(Экспериментально)_** Добавлена возможность управлять системными таблетками базы (SchemeShard, Coordinators, Mediators, SysViewProcessor) её собственному Hive'у, вместо корневого Hive'а, и делать это сразу в момент создания новой базы. Без этого флага системные таблетки новой базы создаются в корневом Hive'е, что может негативно сказаться на его загруженности. Включение этого флага делает базы полностью изолированными по нагрузке, что может быть особенно актуально для инсталляций, состоящих из ста и более узлов. Для включения этой функциональности добавьте `alter_database_create_hive_first: true` в секцию `feature_flags` в [конфигурационный файл](reference/configuration/index.md).

### Исправления ошибок

* Исправлена ошибка в автоконфигурации акторной системы, в результате чего вся нагрузка ложится на системный пул.
* Исправлена ошибка, приводящая к полному сканированию при поиске по префиксу первичного ключа через `LIKE`.
* Исправлены ошибки при взаимодействии с фолловерами даташардов.
* Исправлены ошибки при работе с памятью в колоночных таблицах.
* Исправлена ошибки при обработке условий для immediate-транзакций.
* Исправлена ошибка в работе итераторных чтений на фолловерах даташардов.
* Исправлена ошибка, приводящая к лавинообразной переустановке сессий доставки данных до асинхронных индексов
* Исправлены ошибки в оптимизаторе в сканирующих запросах
* Исправлена ошибка некорректного расчёта потребления хранилища hive'ом после расширения базы
* Исправлена ошибка зависания операций от несуществующих итераторов
* Исправлены ошибки при чтении диапазона на `NOT NULL` колонке
* Исправлена ошибка зависания репликации VDisk'ов
* Исправлена ошибка в работе опции `run_interval` в TTL

## Версия 23.1 {#23-1}

Дата выхода 5 мая 2023. Для обновления до версии 23.1 перейдите в раздел [Загрузки](downloads/index.md#ydb-server).

### Функциональность

* Добавлено [первоначальное сканирование таблицы](concepts/cdc.md#initial-scan) при создании потока изменений CDC. Теперь можно выгрузить все данные, которые существуют на момент создания потока.
* Добавлена возможность [атомарной замены индекса](dev/secondary-indexes.md#atomic-index-replacement). Теперь можно атомарно и прозрачно для приложения подменить один индекс другим заранее созданным индексом. Замена выполняется без простоя.
* Добавлен [аудитный лог](security/audit-log.md) — поток событий, который содержит информацию обо всех операциях над объектами {{ ydb-short-name }}.

### Производительность

* Улучшены форматы передачи данных между стадиями исполнения запроса, что ускорило SELECT на запросах с параметрами на 10%, на операциях записи — до 30%.
* Добавлено [автоматическое конфигурирование](reference/configuration/index.md) пулов акторной системы в зависимости от их нагруженности. Это повышает производительность за счет более эффективного совместного использования ресурсов ЦПУ.
* Оптимизирована логика применения предикатов — выполнение ограничений с использованием OR и IN с параметрами автоматически переносится на сторону DataShard.
* (Экспериментально) Для сканирующих запросов реализована возможность эффективного поиска отдельных строк с использованием первичного ключа или вторичных индексов, что позволяет во многих случаях значительно улучшить производительность. Как и в обычных запросах, для использования вторичного индекса необходимо явно указать его имя в тексте запроса с использованием ключевого слова `VIEW`.
* Реализовано кеширование графа вычисления при выполнении запросов, что уменьшает потребление ЦПУ при его построении.

### Исправления ошибок

* Исправлен ряд ошибок в реализации распределенного хранилища данных. Мы настоятельно рекомендуем всем пользователям обновиться на актуальную версию.
* Исправлена ошибка построения индекса на not null колонках.
* Исправлен подсчет статистики при включенном MVCC.
* Исправлены ошибки с бэкапами.
* Исправлена гонка во время сплита и удаления таблицы с CDC.

## Версия 22.5 {#22-5}

Дата выхода 7 марта 2023. Для обновления до версии **22.5** перейдите в раздел [Загрузки](downloads/index.md#ydb-server).

### Что нового

* Добавлены [параметры конфигурации потока изменения](yql/reference/syntax/alter_table/changefeed.md) для передачи дополнительной информации об изменениях в топик.
* Добавлена поддержка [переименования для таблиц](concepts/datamodel/table.md#rename) с включенным TTL.
* Добавлено [управление временем хранения записей](concepts/cdc.md#retention-period) для потока изменений.

### Исправления ошибок и улучшения

* Исправлена ошибка при вставке 0 строк операцией BulkUpsert.
* Исправлена ошибка при импорте колонок типа Date/DateTime из CSV.
* Исправлена ошибка импорта данных из CSV с разрывом строки.
* Исправлена ошибка импорта данных из CSV с пустыми значениями.
* Улучшена производительность Query Processing (WorkerActor заменен на SessionActor).
* Компактификация DataShard теперь запускается сразу после операций split или merge.

## Версия 22.4 {#22-4}

Дата выхода 12 октября 2022. Для обновления до версии **22.4** перейдите в раздел [Загрузки](downloads/index.md#ydb-server).

### Что нового

* {{ ydb-short-name }} Topics и Change Data Capture (CDC):

  * Представлен новый Topic API. [Топик](concepts/topic.md) {{ ydb-short-name }} — это сущность для хранения неструктурированных сообщений и доставки их различным подписчикам.
  * Поддержка нового Topic API добавлена в [{{ ydb-short-name }} CLI](reference/ydb-cli/topic-overview.md) и [SDK](reference/ydb-sdk/topic.md). Topic API предоставляет методы потоковой записи и чтения сообщений, а также управления топиками.
  * Добавлена возможность [захвата изменений данных таблицы](concepts/cdc.md) с отправкой сообщений об изменениях в топик.

* SDK:

  * Добавлена возможность взаимодействовать с топиками в {{ ydb-short-name }} SDK.
  * Добавлена официальная поддержка драйвера database/sql для работы с {{ ydb-short-name }} в Golang.

* Embedded UI:

  * Поток изменений CDC и вторичные индексы теперь отображаются в иерархии схемы базы данных как отдельные объекты.
  * Улучшена визуализация графического представления query explain планов.
  * Проблемные группы хранения теперь более заметны.
  * Различные улучшения на основе UX-исследований.

* Query Processing:

  * Добавлен Query Processor 2.0 — новая подсистема выполнения OLTP-запросов со значительными улучшениями относительно предыдущей версии.
  * Улучшение производительности записи составило до 60%, чтения до 10%.
  * Добавлена возможность включения ограничения NOT NULL для первичных ключей в YDB во время создания таблиц.
  * Включена поддержка переименования вторичного индекса в режиме онлайн без остановки сервиса.
  * Улучшено представление query explain, которое теперь включает графы для физических операторов.

* Core:

  * Для read-only транзакций добавлена поддержка консистентного снапшота, который не конфликтует с пишущими транзакциями.
  * Добавлена поддержка BulkUpsert для таблиц с асинхронными вторичными индексами.
  * Добавлена поддержка TTL для таблиц с асинхронными вторичными индексами.
  * Добавлена поддержка сжатия при экспорте данных в S3.
  * Добавлен audit log для DDL statements.
  * Поддержана аутентификация со статическими учетными данными.
  * Добавлены системные представления для диагностики производительности запросов.
