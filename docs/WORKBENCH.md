# NppAIAssistant workspace

Project and issue tracker: [GitHub](https://github.com/pingqLIN/NppAIAssistant).
The workspace menu also provides **Help / About** and **GitHub project**.

The menu groups workspace access, conversations, selection actions, and settings/help with separators. The panel's **Menu** button opens the same actions. Service/model selection and task/output choices remain above the conversation. **View** selects formatted or raw replies; editor destination and send preview remain beside the composer.

## Reply display

Formatted view supports Markdown headings, bold, inline code, fenced code, bullet lists, and blockquotes. JSON is parsed before indentation, retaining original numeric and string lexemes. Invalid JSON is not repaired. This is a basic text renderer: tables, nested Markdown, HTML rendering, images, and clickable Markdown links are not supported. It does not fetch external resources.

Raw view retains reply content. Display formatting never changes the original reply used by [editor output](EDITOR_OUTPUT.md). JSON formatting is bounded to 1 MiB and the existing parser's depth/node limits. Valid JSON uses a monospace font. Switching display mode does not make a provider request.

## Request status

Preparing, connecting, sending, waiting, receiving, and validating use a rotating indicator. Success, request error, and connection failure have distinct indicators; terminal animations stop after eight 350 ms ticks. Windows reduced-motion preference disables animation. Text labels remain available. An HTTP error is distinct from a failed send/receive connection. There is no automatic retry or claim that the connection is healthy while idle.

## Built-in prompts

Code repair asks for the smallest correct change without invented dependencies. Refactoring preserves behavior and interfaces. Explanation separates source evidence from assumptions. Test generation calls for runnable normal/boundary/error cases and does not claim execution. Documentation stays within supported facts. Review prioritizes evidence, impact, and minimal fixes. Replacement-only output excludes conflicting Markdown/risk prose. Custom prompt templates remain unchanged and can override built-in defaults.

## Language coverage

| Surface | English | Traditional Chinese | Japanese | Spanish |
| --- | --- | --- | --- | --- |
| Existing `TextId` dictionary | 113 entries | 113 entries | 12 overrides | 12 overrides |
| Main menu, destination, view, request status, help, preset names | Supported | Supported | Supported | Supported |
| Response-language preference | Supported | Supported | Supported | Supported |
| Advanced settings and technical diagnostics | Baseline | Mixed translation/English | English fallback | English fallback |

Dictionary counts are code-level coverage, not an overall completion percentage. Main strings also exist outside `TextId`. Some placeholder, notification, and structured-output labels remain English. Japanese/Spanish are partial UI localizations, not complete translations. Linguistic review by native speakers and live Notepad++ layout acceptance remain pending.
