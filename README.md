[![Build Status](https://travis-ci.com/farafonov-alexey/node-atol-wrapper.svg?branch=master)](https://app.travis-ci.com/farafonov-alexey/node-atol-wrapper)

Название
=========
Node.js обертка для драйвера торгового оборудования (ДТО) 10 от компании [АТОЛ](https://www.atol.ru/)

[ Источник ](https://forum.atol.ru/lofiversion/index.php/t34345.html)

[Документация по интеграции](http://integration.atol.ru/)

Драйвер
========
Обёртка **не содержит драйвер и не связывается с ним при сборке**: библиотека ДТО 10
загружается во время работы — та, что установлена на машине. Сборка не зависит от версии
драйвера, а обёртка работает с любой установленной версией ДТО 10.

Порядок поиска — как у обёрток самого АТОЛ:
1. путь, переданный в `loadLibrary(path)` (каталог или файл библиотеки);
2. каталог исполняемого файла;
3. каталог установки драйвера (Windows: реестр `HKLM\SOFTWARE\ATOL\Drivers\10.0\KKT`,
   значение `INSTALL_DIR` + `\bin`);
4. системные каталоги (`libfptr10.so`, `fptr10.dll`, `fptr10.framework`).

* `loadLibrary([path])` - Загрузить драйвер сейчас; без пути — поиск по порядку выше.
  Возвращает `{ loaded, path, version }`, при неудаче выбрасывает ошибку со списком
  проверенных мест. `create()` загружает драйвер сам при первом вызове.
* `driverInfo()` - Что загружено, ничего не загружая: `{ loaded, path, version }`.

Начиная с ДТО 10.10.8.0 драйвер работает только при запущенной службе АТОЛ Connect (UEM,
`uem-agent`), её ставит инсталлятор драйвера; без неё `open()` вернёт ошибку
«Для работы с драйвером необходимо запустить службу UEMA».

Функции
========
Реализованы следующие методы работы с драйвером
* `create()` -  Инициализация драйвера 
* `destroy()` - Деинициализация драйвера
* `getSettings()` - Выгрузка настроек

Возвращает json-объект настроек, который затем можно передать в функцию setSettings
```js
{ AccessPassword: '',
  AutoDisableBluetooth: false,
  AutoEnableBluetooth: true,
  BaudRate: 115200,
  Bits: 8,
  ComFile: '1',
  IPAddress: '192.168.1.10',
  IPPort: 5555,
  LibraryPath: '',
  MACAddress: 'FF:FF:FF:FF:FF:FF',
  Model: 500,
  OfdChannel: 0,
  Parity: 0,
  Port: 0,
  StopBits: 0,
  UsbDevicePath: 'auto',
  UserPassword: '' }
```
* `setSettings(settings)` - Настройка драйвера

Принимает _settings_ json-объект настроек полученных на предыдущем шаге и модифицированых по необходимости.
* `open()` - Соединение с ККТ
* `close()` - Завершение соединения с ККТ
* `processJson(task)` - Выполнение JSON-задания. Синхронная версия функции.

Здесь _task_ - json-объект описание задания для ККТ. Например, задание для открытия смены:
```js
{
    type: 'openShift',
    operator: {
       name: 'Иванов',
       vatin: '123654789507'
    }
}
```
Более подробную информацию по видам json-задании можно получить из документации к [ДТО 10](http://integration.atol.ru/)
* `processJsonAsync(task, callback)`(от [@maxvgi](https://github.com/maxvgi/node-atol-wrapper)) - Выполнение JSON-задания асинхронно. 
После завершения вызывает функцию callback в порядке error-first. 
Вызывать функцию `close()` необходимо только после завершения всех асинхронных заданий.
#### Обработка ошибок
При возникновении ошибки во время выполнения функций ДТО, она обрабатывается и выбрасывается Error c текстом ошибки, 
например, `Error: Ошибка - 4 [ Порт недоступен ]`. При работе с асинхронными функциями объекты Error возвращаются
первым аргументом при вызове функции callback.

### Поддерживаемые платформы:
* Widows, Linux, MacOS

Пример использования
========
Пример использования обертки можно посмотреть в файле index.js

## Shtrih-M / POScenter driver (fr_drv_ng)

The same module also binds the POScenter driver's "classic" C interface
(`libclassic_fr_drv_ng.so`, `classic_fr_drv_ng.dll`). As with DTO 10, the
driver is not linked or shipped — the installed one is loaded at run time:

```js
const w = require('node-atol-wrapper');
w.loadShtrihLibrary('/opt/fr_drv_ng');            // directory or file; no argument — exe dir, then system path
const o = new w.ShtrihClassic();                  // one context per device
o.call('Connect', { Password: 30, ConnectionURI: 'tcp://192.168.137.111:7778?timeout=15000&protocol=v1' }, [],
  (err, r) => console.log(r.ResultCode, r.ResultCodeDescription));
o.call('ExchangeBytes', { Password: 30, BinaryConversion: 1, TransferBytes: '10 1E 00 00 00' }, ['TransferBytes'], cb);
o.destroy();
```

`call(method, props, outNames, cb)` sets the properties, runs the method in a
worker thread and returns `ResultCode`, `ResultCodeDescription` and the listed
properties. Property names and types are in `src/shtrih_api.inc`, generated from
the driver's `c_classic_interface.h` by `scripts/gen-shtrih-api.py`. While a
device is connected, the driver's TS PIoT module (`libmp_ts_piot.so`) serves
gRPC on port 4042 from this process.
