# gcp-shell
Easy-open an SSH terminal to a GCP compute instance without a public IP address

# Example

`gcp-shell` executes commands using the `gcloud` executable. This means
you must be logged in and must have a GCP project configured.

```bash
$ gcloud auth login

$ gcloud config set project my-project
# or set $CLOUDSDK_CORE_PROJECT

$ gcloud config set compute/zone ZONE
# or set $CLOUDSDK_COMPUTE_ZONE
# or use the `-z ZONE` option to `gcp-shell`

$ gcp-shell instance-name
# or: gcp-shell user@instance-1
```

# Usage

```
Usage: gcp-shell [-z zone] [-p localPort=2020] [user@]instance

  Open an IAP tunnel to GCP compute <instance> on local-port <localPort>.
  Then open an ssh connection, and close the tunnel when the
  conneciton is complete.

  The zone may be specified by the '-z' argument or by the usual
  GCP configuration options.
```
