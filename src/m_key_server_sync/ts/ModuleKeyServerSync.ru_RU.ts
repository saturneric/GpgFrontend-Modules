<?xml version="1.0" encoding="utf-8"?>
<!DOCTYPE TS>
<TS version="2.1" language="ru_RU">
<context>
    <name>GTrC</name>
    <message>
        <location filename="../KeyServerSyncModule.cpp" line="252"/>
        <source>The following email addresses have status:
</source>
        <translation>Статус следующих адресов электронной почты:</translation>
    </message>
    <message>
        <location filename="../KeyServerSyncModule.cpp" line="262"/>
        <source>Could not parse status information.</source>
        <translation>Не удалось обработать информацию о статусе.</translation>
    </message>
    <message>
        <location filename="../KeyServerSyncModule.cpp" line="230"/>
        <location filename="../KeyServerSyncModule.cpp" line="273"/>
        <source>Public Key Upload Successful</source>
        <translation>Открытый ключ успешно загружен</translation>
    </message>
    <message>
        <source>The public key was successfully uploaded to the key server keys.openpgp.org.
Fingerprint: %1

%2
Please check your email (%3) for further verification from keys.openpgp.org.

Note: For verification, you can find more information here: https://keys.openpgp.org/about</source>
        <translation type="vanished">Открытый ключ успешно загружен на сервер ключей keys.openpgp.org.
Отпечаток: %1

%2
Пожалуйста, проверьте вашу электронную почту (%3) на наличие дальнейшего подтверждения от keys.openpgp.org.

Примечание: дополнительную информацию о проверке можно найти здесь: https://keys.openpgp.org/about</translation>
    </message>
    <message>
        <location filename="../KeyServerSyncModule.cpp" line="189"/>
        <location filename="../KeyServerSyncModule.cpp" line="217"/>
        <location filename="../KeyServerSyncModule.cpp" line="289"/>
        <source>Key Upload Failed</source>
        <translation>Ошибка загрузки ключа</translation>
    </message>
    <message>
        <location filename="../KeyServerSyncModule.cpp" line="218"/>
        <location filename="../KeyServerSyncModule.cpp" line="290"/>
        <source>Failed to upload public key to the server.
Fingerprint: %1
Error: %2</source>
        <translation>Не удалось загрузить открытый ключ на сервер.
Отпечаток: %1
Ошибка: %2</translation>
    </message>
    <message>
        <location filename="../KeyServerSyncModule.cpp" line="325"/>
        <source>Key Update Failed</source>
        <translation>Ошибка обновления ключа</translation>
    </message>
    <message>
        <source>Failed to retrieve public key from the server.
Key ID: %1
Error: %2</source>
        <translation type="vanished">Не удалось получить открытый ключ с сервера.
Идентификатор ключа: %1
Ошибка: %2</translation>
    </message>
    <message>
        <location filename="../KeyServerSyncModule.cpp" line="101"/>
        <source>The key server did not return a key.</source>
        <translation>Сервер ключей не вернул ключ.</translation>
    </message>
    <message>
        <location filename="../KeyServerSyncModule.cpp" line="127"/>
        <source>Publish Without Verification?</source>
        <translation>Опубликовать без проверки?</translation>
    </message>
    <message>
        <location filename="../KeyServerSyncModule.cpp" line="129"/>
        <source>%1 does not support verified publishing (VKS), so the key would be uploaded over HKP instead.

The server will not confirm your email address, and the upload cannot be undone — HKP key servers do not let keys be removed.

Publish to %1 anyway?</source>
        <translation>%1 не поддерживает проверенную публикацию (VKS), поэтому ключ будет загружен через HKP.

Сервер не подтвердит ваш адрес электронной почты, и загрузку нельзя отменить — серверы ключей HKP не позволяют удалять ключи.

Опубликовать на %1 всё равно?</translation>
    </message>
    <message>
        <location filename="../KeyServerSyncModule.cpp" line="190"/>
        <source>Failed to export the public key before uploading.
Key: %1</source>
        <translation>Не удалось экспортировать открытый ключ перед загрузкой.
Ключ: %1</translation>
    </message>
    <message>
        <location filename="../KeyServerSyncModule.cpp" line="232"/>
        <source>The public key was uploaded to the key server %2 over HKP.
Fingerprint: %1</source>
        <translation>Открытый ключ загружен на сервер ключей %2 через HKP.
Отпечаток: %1</translation>
    </message>
    <message>
        <location filename="../KeyServerSyncModule.cpp" line="274"/>
        <source>The public key was successfully uploaded to the key server %4.
Fingerprint: %1

%2
Please check your email (%3) for further verification from %4.</source>
        <translation>Открытый ключ успешно загружен на сервер ключей %4.
Отпечаток: %1

%2
Пожалуйста, проверьте вашу электронную почту (%3) на наличие дальнейшего подтверждения от %4.</translation>
    </message>
    <message>
        <location filename="../KeyServerSyncModule.cpp" line="326"/>
        <source>Failed to retrieve public key from %3.
Key ID: %1
Error: %2</source>
        <translation>Не удалось получить открытый ключ с %3.
Идентификатор ключа: %1
Ошибка: %2</translation>
    </message>
    <message>
        <location filename="../KeyServerSyncModule.cpp" line="375"/>
        <source>Key Server</source>
        <translation>Сервер ключей</translation>
    </message>
    <message>
        <location filename="../KeyServerSyncModule.cpp" line="376"/>
        <source>Import public keys from a trusted key server.</source>
        <translation>Импортировать открытые ключи с доверенного сервера ключей.</translation>
    </message>
    <message>
        <location filename="../KeyServerSyncModule.cpp" line="424"/>
        <source>Key Server Operations</source>
        <translation>Операции с сервером ключей</translation>
    </message>
    <message>
        <location filename="../KeyServerSyncModule.cpp" line="429"/>
        <source>Publish Public Key to Key Server</source>
        <translation>Опубликовать открытый ключ на сервере ключей</translation>
    </message>
    <message>
        <location filename="../KeyServerSyncModule.cpp" line="436"/>
        <source>Refresh Public Key From Key Server</source>
        <translation>Обновить открытый ключ с сервера ключей</translation>
    </message>
    <message>
        <location filename="../KeyServerProbe.cpp" line="247"/>
        <source>The server could not be reached.</source>
        <translation>Не удалось связаться с сервером.</translation>
    </message>
    <message>
        <location filename="../KeyServerProbe.cpp" line="249"/>
        <source>The server responded, but not as a key server: it supports neither the HKP nor the VKS interface.</source>
        <translation>Сервер ответил, но не как сервер ключей: он не поддерживает ни интерфейс HKP, ни интерфейс VKS.</translation>
    </message>
</context>
<context>
    <name>KeyServerSettingsPage</name>
    <message>
        <location filename="../KeyServerSettingsPage.ui" line="17"/>
        <location filename="../KeyServerSettingsPage.cpp" line="58"/>
        <source>Key Server List</source>
        <translation>Список серверов ключей</translation>
    </message>
    <message>
        <location filename="../KeyServerSettingsPage.ui" line="49"/>
        <location filename="../KeyServerSettingsPage.cpp" line="59"/>
        <source>Add a Key Server</source>
        <translation>Добавить сервер ключей</translation>
    </message>
    <message>
        <location filename="../KeyServerSettingsPage.ui" line="60"/>
        <location filename="../KeyServerSettingsPage.cpp" line="62"/>
        <source>Add</source>
        <translation>Добавить</translation>
    </message>
    <message>
        <location filename="../KeyServerSettingsPage.ui" line="82"/>
        <location filename="../KeyServerSettingsPage.cpp" line="60"/>
        <source>Operations</source>
        <translation>Операции</translation>
    </message>
    <message>
        <location filename="../KeyServerSettingsPage.ui" line="88"/>
        <location filename="../KeyServerSettingsPage.cpp" line="63"/>
        <source>Set As Default</source>
        <translation>Установить по умолчанию</translation>
    </message>
    <message>
        <location filename="../KeyServerSettingsPage.ui" line="95"/>
        <location filename="../KeyServerSettingsPage.cpp" line="64"/>
        <source>Test Selected</source>
        <translation>Проверить выбранные</translation>
    </message>
    <message>
        <location filename="../KeyServerSettingsPage.ui" line="102"/>
        <location filename="../KeyServerSettingsPage.cpp" line="65"/>
        <source>Delete Selected</source>
        <translation>Удалить выбранные</translation>
    </message>
    <message>
        <location filename="../KeyServerSettingsPage.cpp" line="67"/>
        <source>https://keys.example.org</source>
        <translation>https://keys.example.org</translation>
    </message>
    <message>
        <location filename="../KeyServerSettingsPage.cpp" line="68"/>
        <source>A new key server is tested against the HKP and VKS interfaces before it is added. Searching uses HKP. Publishing and refreshing always use the default server: over VKS where it offers it, over HKP otherwise.</source>
        <translation>Новый сервер ключей проверяется через интерфейсы HKP и VKS перед добавлением. Поиск использует HKP. Публикация и обновление всегда используют сервер по умолчанию: через VKS, если он его поддерживает, иначе через HKP.</translation>
    </message>
    <message>
        <location filename="../KeyServerSettingsPage.cpp" line="75"/>
        <source>Default</source>
        <translation>По умолчанию</translation>
    </message>
    <message>
        <location filename="../KeyServerSettingsPage.cpp" line="75"/>
        <source>Address</source>
        <translation>Адрес</translation>
    </message>
    <message>
        <location filename="../KeyServerSettingsPage.cpp" line="75"/>
        <source>HKP</source>
        <translation>HKP</translation>
    </message>
    <message>
        <location filename="../KeyServerSettingsPage.cpp" line="75"/>
        <source>VKS</source>
        <translation>VKS</translation>
    </message>
    <message>
        <location filename="../KeyServerSettingsPage.cpp" line="75"/>
        <source>Status</source>
        <translation>Статус</translation>
    </message>
    <message>
        <location filename="../KeyServerSettingsPage.cpp" line="76"/>
        <source>Last Tested</source>
        <translation>Последняя проверка</translation>
    </message>
    <message>
        <location filename="../KeyServerSettingsPage.cpp" line="108"/>
        <source>yes</source>
        <translation>да</translation>
    </message>
    <message>
        <location filename="../KeyServerSettingsPage.cpp" line="109"/>
        <source>no</source>
        <translation>нет</translation>
    </message>
    <message>
        <location filename="../KeyServerSettingsPage.cpp" line="127"/>
        <source>Verified</source>
        <translation>Проверено</translation>
    </message>
    <message>
        <location filename="../KeyServerSettingsPage.cpp" line="127"/>
        <source>Unverified</source>
        <translation>Не проверено</translation>
    </message>
    <message>
        <location filename="../KeyServerSettingsPage.cpp" line="136"/>
        <source>never</source>
        <translation>никогда</translation>
    </message>
    <message>
        <location filename="../KeyServerSettingsPage.cpp" line="162"/>
        <source>Invalid Address</source>
        <translation>Недопустимый адрес</translation>
    </message>
    <message>
        <location filename="../KeyServerSettingsPage.cpp" line="163"/>
        <source>&quot;%1&quot; is not a valid key server address.</source>
        <translation>&quot;%1&quot; не является допустимым адресом сервера ключей.</translation>
    </message>
    <message>
        <location filename="../KeyServerSettingsPage.cpp" line="173"/>
        <source>Already Listed</source>
        <translation>Уже в списке</translation>
    </message>
    <message>
        <location filename="../KeyServerSettingsPage.cpp" line="174"/>
        <source>%1 is already in the key server list.</source>
        <translation>%1 уже есть в списке серверов ключей.</translation>
    </message>
    <message>
        <location filename="../KeyServerSettingsPage.cpp" line="180"/>
        <source>Insecure Key Server Address</source>
        <translation>Небезопасный адрес сервера ключей</translation>
    </message>
    <message>
        <location filename="../KeyServerSettingsPage.cpp" line="181"/>
        <source>%1 uses plain HTTP, so anyone on the network can see and change what you look up or publish. Add it anyway?</source>
        <translation>%1 использует обычный HTTP, поэтому любой в сети может видеть и изменять то, что вы ищете или публикуете. Всё равно добавить?</translation>
    </message>
    <message>
        <location filename="../KeyServerSettingsPage.cpp" line="229"/>
        <source>No Verified Publishing</source>
        <translation>Нет подтверждённой публикации</translation>
    </message>
    <message>
        <location filename="../KeyServerSettingsPage.cpp" line="230"/>
        <source>%1 does not support the VKS interface, so publishing and refreshing will use HKP instead.

Over HKP the server does not confirm your email address, and an uploaded key cannot be removed again.

Use %1 as the default anyway?</source>
        <translation>%1 не поддерживает интерфейс VKS, поэтому публикация и обновление будут использовать HKP.

Через HKP сервер не подтверждает ваш адрес электронной почты, и загруженный ключ нельзя будет удалить.

Всё равно использовать %1 по умолчанию?</translation>
    </message>
    <message>
        <location filename="../KeyServerSettingsPage.cpp" line="291"/>
        <source>Key Server Not Verified</source>
        <translation>Сервер ключей не проверен</translation>
    </message>
    <message>
        <location filename="../KeyServerSettingsPage.cpp" line="292"/>
        <source>%1 did not answer as a key server.

%2

It has been added and marked unverified; use Test Selected to try again.</source>
        <translation>%1 не ответил как сервер ключей.

%2

Он был добавлен и помечен как непроверенный; используйте «Проверить выбранное», чтобы попробовать снова.</translation>
    </message>
</context>
<context>
    <name>SearchKeyDialog</name>
    <message>
        <location filename="../SearchKeyDialog.ui" line="14"/>
        <source>Search Keys</source>
        <translation>Поиск ключей</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.ui" line="36"/>
        <source>Key Server</source>
        <translation>Сервер ключей</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.ui" line="56"/>
        <source>Search Type</source>
        <translation>Тип поиска</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.ui" line="66"/>
        <source>Search Value</source>
        <translation>Искомое значение</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.ui" line="85"/>
        <source>Search</source>
        <translation>Найти</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.ui" line="105"/>
        <source>Tips: double click to import the selected key.</source>
        <translation>Совет: дважды щёлкните, чтобы импортировать выбранный ключ.</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.cpp" line="110"/>
        <source>Key ID</source>
        <translation>Идентификатор ключа</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.cpp" line="110"/>
        <source>UID</source>
        <translation>UID</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.cpp" line="110"/>
        <source>Creation Date</source>
        <translation>Дата создания</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.cpp" line="111"/>
        <source>Expiration Date</source>
        <translation>Дата истечения срока</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.cpp" line="111"/>
        <source>Algorithm</source>
        <translation>Алгоритм</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.cpp" line="111"/>
        <source>Key Size</source>
        <translation>Размер ключа</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.cpp" line="112"/>
        <source>Status</source>
        <translation>Статус</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.cpp" line="115"/>
        <source>By Key ID</source>
        <translation>По идентификатору ключа</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.cpp" line="116"/>
        <source>By Email</source>
        <translation>По электронной почте</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.cpp" line="117"/>
        <source>By Fingerprint</source>
        <translation>По отпечатку</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.cpp" line="120"/>
        <source>Enter a value, then press Enter or Search</source>
        <translation>Введите значение, затем нажмите Enter или Поиск</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.cpp" line="163"/>
        <source>Search value is empty.</source>
        <translation>Значение поиска не введено.</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.cpp" line="179"/>
        <source>Key server URL is empty.</source>
        <translation>Адрес сервера ключей не указан.</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.cpp" line="188"/>
        <source>Invalid key server URL format.</source>
        <translation>Неверный формат адреса сервера ключей.</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.cpp" line="201"/>
        <source>Invalid email format.</source>
        <translation>Неверный формат адреса электронной почты.</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.cpp" line="216"/>
        <source>Invalid fingerprint format. It should be a hex string of length 16, 40 or 64.</source>
        <translation>Неверный формат отпечатка. Он должен быть шестнадцатеричной строкой длиной 16, 40 или 64.</translation>
    </message>
    <message>
        <source>Invalid fingerprint format. It should be a hex string of length 16 or 40.</source>
        <translation type="vanished">Неверный формат отпечатка. Он должен быть шестнадцатеричной строкой длиной 16 или 40 символов.</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.cpp" line="230"/>
        <source>Invalid Key ID format. It should be a hex string of length 8 or 16.</source>
        <translation>Неверный формат идентификатора ключа. Он должен быть шестнадцатеричной строкой длиной 8 или 16 символов.</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.cpp" line="236"/>
        <source>Unknown search type.</source>
        <translation>Неизвестный тип поиска.</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.cpp" line="273"/>
        <location filename="../SearchKeyDialog.cpp" line="283"/>
        <source>No keys found matching your search.</source>
        <translation>Не найдено ключей, соответствующих вашему поиску.</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.cpp" line="302"/>
        <source>(no user ID published)</source>
        <translation>(идентификатор пользователя не опубликован)</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.cpp" line="306"/>
        <source>Unknown</source>
        <translation>Неизвестно</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.cpp" line="310"/>
        <source>Never</source>
        <translation>Никогда</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.cpp" line="374"/>
        <source>No GPG context is available.</source>
        <translation>Контекст GPG недоступен.</translation>
    </message>
</context>
</TS>
