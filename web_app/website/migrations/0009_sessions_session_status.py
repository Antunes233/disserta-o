# Generated by Django 4.2.11 on 2024-06-20 15:33

from django.db import migrations, models


class Migration(migrations.Migration):

    dependencies = [
        ("website", "0008_sessions_session_time"),
    ]

    operations = [
        migrations.AddField(
            model_name="sessions",
            name="session_status",
            field=models.CharField(default="Ongoing", max_length=10, null=True),
        ),
    ]
