# Write AI replies to the editor

Choose **Output to** above the preview/send controls before submitting a chat:

- **Chat panel** (default): keep the reply in the conversation.
- **Insert at cursor**: insert at the cursor captured when submitting; selected text is preserved.
- **Replace selection**: replace a single, non-rectangular selection. An empty selection blocks submission.
- **New document**: open an unsaved document and write the successful reply there.

All successful replies remain in the conversation. Below the transcript, choose a reply and press **Insert reply** to insert it at the current editor cursor. This action preserves selected text. Output is inserted as returned, including Markdown fences when present.

The destination is retained for this execution and can be changed before each request. Changing it while a request is running affects the next request. Preview remains independently configurable.

Automatic insertion/replacement captures the original document and position before the prompt preview. If its contents, encoding, or document identity change, writing is blocked and the reply remains available in the panel. Read-only documents and lossy encoding conversions are also blocked. Existing-document output supports a single selection and documents up to 16 MiB; use a new document for larger sources. Each successful write uses one undo group and never saves the file automatically. Failed or invalid model responses are never written.
