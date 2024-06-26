# Поисковый сервер
Поиск в текстовых документах по ключевым словам с ранжированием по релевантности и рейтингу. Реализован как последовательный, так и параллельный поиск документов.

Поисковый сервер учитывает:
* Стоп слова - слова, которые встречаются во многих документах и обычно не несут смысловой нагрузки. Обычно это предлоги, местоимения, междомения, союзы и цифры. Можно указать набор стоп-слов, и тогда они будут исключены из поиска, что сделает результаты более точными.
* Минус-слова - при наличии такого слова, документ исключается из поиска.

В нестандартных ситуациях программа ведет себя так:
* Если запрос состоит только из стоп и минус слов, ничего найтись не должно.
* Если одно и то же слово будет в запросе и с минусом, и без, считается, что оно есть только с минусом.
* Стоп-слово исключается из поиска, даже если оно с минусом.

## Инструкция по использованию
Можно запустить в Microsoft Visual Studio или в любой другой среде разработки.

Файл main.cpp содержит тесты, проверяющие работу проекта.

## Системные требования
C++17
