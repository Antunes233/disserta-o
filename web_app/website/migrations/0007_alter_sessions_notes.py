# Generated by Django 4.2.11 on 2024-06-20 12:19

from django.db import migrations, models


class Migration(migrations.Migration):

    dependencies = [
        ("website", "0006_sessions_notes"),
    ]

    operations = [
        migrations.AlterField(
            model_name="sessions",
            name="notes",
            field=models.TextField(blank=True, null=True),
        ),
    ]
