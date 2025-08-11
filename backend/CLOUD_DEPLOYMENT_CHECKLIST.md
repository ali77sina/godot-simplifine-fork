# Cloud Vector Search Deployment Checklist

## Pre-Deployment

### 1. GCP Project Setup
- [ ] Create GCP project or use existing
- [ ] Enable billing
- [ ] Note project ID: `_________________`

### 2. Enable APIs
```bash
gcloud services enable aiplatform.googleapis.com
gcloud services enable bigquery.googleapis.com
gcloud services enable cloudbuild.googleapis.com
gcloud services enable run.googleapis.com
gcloud services enable secretmanager.googleapis.com
```

### 3. Set Up Authentication
- [ ] Configure OAuth consent screen
- [ ] Add authorized redirect URIs:
  - `http://localhost:8000/auth/callback` (dev)
  - `https://gamechat.simplifine.com/auth/callback` (prod)
- [ ] Get OAuth client ID and secret

### 4. Environment Variables
Create `.env` file:
```
OPENAI_API_KEY=sk-...
GCP_PROJECT_ID=your-project-id
GOOGLE_CLIENT_ID=...
GOOGLE_CLIENT_SECRET=...
GITHUB_CLIENT_ID=...
GITHUB_CLIENT_SECRET=...
FLASK_SECRET_KEY=...
```

## Deployment

### 1. Deploy Backend
```bash
cd backend
./deploy.sh your-project-id
```

### 2. Verify Deployment
- [ ] Check Cloud Run: https://console.cloud.google.com/run
- [ ] Test health endpoint: `curl https://your-service-url/health`
- [ ] Check logs for errors

### 3. Verify Permissions
The Cloud Run service account needs:
- `roles/bigquery.dataEditor` - Write to BigQuery
- `roles/bigquery.user` - Query BigQuery
- `roles/aiplatform.user` - Use Vertex AI
- `roles/secretmanager.secretAccessor` - Read secrets

## Post-Deployment

### 1. Test Indexing
```bash
curl -X POST https://your-service-url/embed \
  -H "Content-Type: application/json" \
  -H "Authorization: Bearer YOUR_TOKEN" \
  -d '{
    "action": "index_project",
    "project_root": "/test/path"
  }'
```

### 2. Monitor Resources
- **BigQuery**: Check dataset creation
  ```sql
  SELECT COUNT(*) FROM `project.godot_embeddings.embeddings`
  ```
- **Cloud Run**: Monitor metrics
- **Vertex AI**: Check API usage

### 3. Set Up Alerts
- [ ] Cloud Run error rate > 1%
- [ ] BigQuery storage > 1GB
- [ ] Monthly cost > $50

## Troubleshooting

### "Permission denied" errors
```bash
# Grant permissions to service account
PROJECT_NUMBER=$(gcloud projects describe PROJECT_ID --format="value(projectNumber)")
SA="${PROJECT_NUMBER}-compute@developer.gserviceaccount.com"

gcloud projects add-iam-policy-binding PROJECT_ID \
  --member="serviceAccount:${SA}" \
  --role="roles/bigquery.dataEditor"
```

### "API not enabled" errors
```bash
# Enable missing API
gcloud services enable SERVICE_NAME.googleapis.com
```

### "Table not found" errors
- BigQuery table is created on first index
- Check if dataset exists: `godot_embeddings`
- Manually create if needed

### High latency
- Check Cloud Run instance count
- Increase memory/CPU if needed
- Consider regional deployment

## Cost Optimization

### 1. BigQuery
- Set up table expiration for old embeddings
- Use partitioning by indexed_at timestamp
- Monitor storage growth

### 2. Vertex AI
- Batch embedding requests
- Cache common queries
- Consider rate limiting

### 3. Cloud Run
- Set min instances to 0
- Use appropriate memory (1GB usually enough)
- Enable CPU boost for cold starts

## Security Checklist

- [ ] OAuth properly configured
- [ ] Secrets in Secret Manager (not env vars)
- [ ] HTTPS only (no HTTP)
- [ ] CORS configured for Godot client
- [ ] Rate limiting implemented
- [ ] User data properly isolated

## Monitoring Dashboard

Create custom dashboard with:
1. Cloud Run request count
2. BigQuery storage usage
3. Vertex AI API calls
4. Error rate by service
5. Cost breakdown by service

## Backup Strategy

1. **BigQuery**: Enable time travel (7 days default)
2. **Export embeddings**: Weekly BigQuery export to GCS
3. **Document hashes**: Store file hashes for deduplication

## Update Process

1. Test changes locally
2. Deploy to staging (if available)
3. Run integration tests
4. Deploy to production
5. Monitor for 30 minutes
6. Roll back if issues

## Contact for Issues

- Cloud Run logs: `gcloud logs read --service=godot-ai-backend`
- BigQuery logs: Check in Cloud Console
- Vertex AI logs: Check API metrics
- File GitHub issue for bugs



