"""Keep the public Linux beta issue intake small and structurally valid."""

from pathlib import Path
import unittest

import yaml


ROOT = Path(__file__).resolve().parents[1]
TEMPLATES = ROOT / ".github" / "ISSUE_TEMPLATE"


class IssueFormTests(unittest.TestCase):
    def test_three_distinct_forms_with_valid_fields(self):
        forms = sorted(TEMPLATES.glob("*.yml"))
        self.assertEqual([f.name for f in forms], [
            "bug_report.yml", "compatibility.yml", "config.yml", "feature_request.yml",
        ])
        seen_names = set()
        for path in forms:
            if path.name == "config.yml":
                continue
            with self.subTest(path=path.name):
                form = yaml.safe_load(path.read_text())
                self.assertTrue(form["name"])
                self.assertTrue(form["description"])
                self.assertNotIn(form["name"], seen_names)
                seen_names.add(form["name"])
                self.assertIn("triage", form["labels"])
                self.assertGreaterEqual(len(form["body"]), 3)
                ids = [field.get("id") for field in form["body"] if "id" in field]
                self.assertEqual(len(ids), len(set(ids)))
                self.assertTrue(any(
                    field["type"] == "checkboxes" and field.get("id") == "privacy"
                    for field in form["body"]
                ))
                for field in form["body"]:
                    self.assertIn(field["type"], {"markdown", "input", "textarea", "dropdown", "checkboxes"})
                    self.assertIn("attributes", field)

    def test_chooser_disables_blank_public_issues_and_offers_private_route(self):
        config = yaml.safe_load((TEMPLATES / "config.yml").read_text())
        self.assertIs(config["blank_issues_enabled"], False)
        self.assertTrue(any(
            link["url"] == "https://tapzap.app/contact"
            for link in config["contact_links"]
        ))

    def test_readme_and_security_note_keep_private_material_out(self):
        readme = (ROOT / "README.md").read_text()
        security = (ROOT / "SECURITY.md").read_text()
        self.assertIn("issues/new/choose", readme)
        self.assertIn("license keys", readme)
        self.assertIn("https://tapzap.app/contact", security)
        self.assertIn("Do not", security)


if __name__ == "__main__":
    unittest.main()
