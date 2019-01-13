# IMAP Module
| Since  | Origin / Contributor  | Maintainer  | Source  |
| :----- | :-------------------- | :---------- | :------ |
| 2015-03-12 | [AllAboutEE](https://github.com/AllAboutEE) | [AllAboutEE](https://github.com/AllAboutEE) | [imap.lua](../../lua_modules/email/imap.lua) |

This Lua module provides a simple implementation of an [IMAP 4rev1](http://www.faqs.org/rfcs/rfc2060.html) protocol that can be used to read e-mails.


### Require
```lua
imap = require("imap.lua")
```

### Release
```lua
imap = nil
package.loaded["imap"] = nil
```

## imap.response_processed()
Function used to check if IMAP command was processed.

#### Syntax
`imap.response_processed()`

#### Parameters
None

#### Returns
The response process status of the last IMAP command sent. If return value is `true` it means the command was processed.


## imap.config()
Initiates the IMAP settings.

#### Syntax
`imap.config(username, password, tag, [debug])`

#### Parameters
- `username`: IMAP username. For most e-mail providers e-mail address is used as username.
- `password`: IMAP password.
- `tag`: IMAP tag. With current implementation any tag like "t1" should work.
- `debug`: (boolean) if set to true entire conversation between the ESP8266 and IMAP server will be shown. Default setting is false.

#### Returns
`nil`

## imap.login()
Logs into a new email session.

#### Syntax
`imap.login(socket)`

#### Parameters
- `socket`: IMAP TCP socket object created using `net.createConnection`

#### Returns
`nil`

## imap.get_most_recent_num()
Function to check the most recent email number. Should only be called after `examine` function.

#### Syntax
`imap.get_most_recent_num()`

#### Parameters
None

#### Returns
The most recent email number.

## imap.examine()
IMAP examines the given mailbox/folder. Sends the IMAP EXAMINE command.

#### Syntax
`imap.examine(socket, mailbox)`

#### Parameters
- `socket`: IMAP TCP socket object created using `net.createConnection`
- `mailbox`: E-mail folder name to examine like example `"INBOX"`

#### Returns
`nil`

## imap.get_header()
Function that gets the last fetched header field.

#### Syntax
`imap.get_header()`

#### Parameters
None

#### Returns
The last fetched header field.

## imap.fetch_header()
Fetches an e-mails header field e.g. SUBJECT, FROM, DATE.

#### Syntax
`imap.fetch_header(socket, msg_number, field)`

#### Parameters
- `socket`: IMAP TCP socket object created using `net.createConnection`
- `msg_number`: The email number to read e.g. 1 will read fetch the latest/newest email
- `field`: A header field such as SUBJECT, FROM, or DATE
#### Returns
`nil`

## imap.get_body()
Function to get the last email read's body.

#### Syntax
`imap.get_body()`

#### Parameters
None

#### Returns
The last email read's body.

## imap.fetch_body_plain_text()
Sends the IMAP command to fetch a plain text version of the email's body.

#### Syntax
`imap.fetch_body_plain_text(socket, msg_number)`

#### Parameters
- `socket`: IMAP TCP socket object created using `net.createConnection`
- `msg_number`: The email number to obtain e.g. 1 will obtain the latest email.

#### Returns
`nil`

## imap.logout()
Sends the IMAP command to logout of the email session.

#### Syntax
`imap.logout(socket)`

#### Parameters
- `socket`: IMAP TCP socket object created using `net.createConnection`

#### Returns
`nil`

#### Example
Example use of `imap` module can be found in  [read_email_imap.lua](../../lua_examples/email/read_email_imap.lua) file.
