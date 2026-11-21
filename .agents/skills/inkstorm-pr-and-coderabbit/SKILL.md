---
name: inkstorm-pr-and-coderabbit
description: >-
  INKSTORM のブランチ・PR を準備し、GitHub CLI で CodeRabbit のレビューを取得して分類、修正、返信するときに使う。
---

# PR 準備と CodeRabbit レビュー

1. タスク単位のブランチと差分を確認し、**ローカルでは変更範囲の最小ビルド・関連テスト**（描画変更時は視覚確認、`flake.nix` 変更時は Nix 評価）のみ通す。
   フルの ASan/CTest・`format-check`・`lint`・`dist` は GitHub Actions CI に委譲し、ローカルで重複実行しない。コマンドは [AGENTS.md](../../../AGENTS.md) を参照する。
2. コミットが依頼範囲に含まれる場合は Conventional Commits を使う。push と `gh pr create` は許可された場合に行い、
   PR 本文には変更範囲、設計上の判断、ローカル確認内容・CI 委譲項目を書く。
3. **CodeRabbit レビューの必須トリガ（マージ前ゲート）**:
   本リポジトリはスター数条件により CodeRabbit の自動レビューがスキップされる場合があるため、PR 作成直後（または CodeRabbit チェックが `SKIPPED` のとき）に必ず次のコマンドでレビューを起動する。

```sh
gh pr comment "$pr" --body "@coderabbitai review"
```

4. PR 番号を調べ、対象の head に対する **CI 全チェック、自身のコードレビュー、および CodeRabbit レビューの完了と指摘**をすべて確認する。CodeRabbit レビューが完了する前、または Must Fix / Should Fix の未対応が残る状態ではマージしない。

```sh
gh pr checks "$pr"
gh pr view "$pr" --json headRefOid,reviewDecision,reviews,comments,statusCheckRollup
gh api --paginate "repos/{owner}/{repo}/pulls/$pr/reviews"
gh api --paginate "repos/{owner}/{repo}/pulls/$pr/comments"
gh api --paginate "repos/{owner}/{repo}/issues/$pr/comments"
```

レビュー本文、行コメント、要約を突き合わせる。対象コミットが古い指摘は現行コードで再確認する。
CodeRabbit の提案をそのまま適用せず、コード、仕様、再現結果にもとづいて分類する。

| 分類 | 判断と対応 |
| --- | --- |
| Must Fix | 実バグ、メモリ安全性・UB、依存違反、決定性の破壊、配布 zip への開発専用ファイル混入を修正する。 |
| Should Fix | スコープ内の明確なエッジケース、バグ予防、可読性の改善を今回対応する。 |
| Defer / Won't Fix | 過剰抽象化、フェーズ外、規模に合わない最適化、誤検知の根拠を残す。 |
| Verify First | 根拠が曖昧な指摘は実装・仕様・再現テストで裏を取り、上の3分類に確定する。 |

Must Fix と Should Fix の対応だけを差分に入れ、修正箇所の最小確認後に push して CI で全体検証する（CI が落ちた場合のみ該当チェックをローカル再現する）。
テストを弱める変更は禁止。見送りや誤検知は該当コメントへの PR 返信に理由と根拠を簡潔に記録する。返信の許可を待つ場合は文案を手元に残す。
