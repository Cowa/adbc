# To support a new version of ADBC, you must:
#
# 1. Update the contents in 3rd_party with VERSION (only root files and c/)
# 2. Invoke `elixir update.exs VERSION`
# 3. ...
# 4. Profit!
#
Mix.install([{:req, "~> 0.4"}])

drivers = ~w(sqlite postgresql flightsql snowflake)a
file = "lib/adbc_driver.ex"

version =
  case System.argv() do
    [version] -> version
    _ -> raise "expected VERSION as argument"
  end

opts =
  if token = System.get_env("GITHUB_TOKEN") do
    [auth: {:bearer, token}]
  else
    []
  end

release =
  Req.get!(
    "https://api.github.com/repos/apache/arrow-adbc/releases/tags/apache-arrow-adbc-#{version}",
    opts
  )

if release.status != 200 do
  raise "unknown GitHub release for version #{version}\n\n#{inspect(release)}"
end

assets = release.body["assets"]

mapping =
  for driver <- drivers, into: %{} do
    prefix = "adbc_driver_#{driver}-#{version}"
    suffix = ".whl"

    wheels =
      Enum.filter(assets, fn %{"name" => name} ->
        String.starts_with?(name, prefix) and String.ends_with?(name, suffix)
      end)

    data_for = fn wheels, parts ->
      case Enum.split_with(wheels, fn %{"name" => name} -> Enum.all?(parts, &(name =~ &1)) end) do
        {[%{"browser_download_url" => url}], rest} -> {%{url: url}, rest}
        {[], _rest} -> raise "no wheel matching #{inspect(parts)}\n\n#{inspect(wheels)}"
        {_, _rest} -> raise "many wheels matching #{inspect(parts)}\n\n#{inspect(wheels)}"
      end
    end

    {aarch64_apple_darwin, wheels} = data_for.(wheels, ["macosx", "arm64"])
    {x86_64_apple_darwin, wheels} = data_for.(wheels, ["macosx", "x86_64"])
    {aarch64_linux_gnu, wheels} = data_for.(wheels, ["manylinux", "aarch64"])
    {x86_64_linux_gnu, wheels} = data_for.(wheels, ["manylinux", "x86_64"])
    {x86_64_windows_msvc, wheels} = data_for.(wheels, ["win_amd64"])

    if wheels != [] do
      IO.puts("The following wheels for #{driver} are not being used:\n\n#{inspect(wheels)}")
    end

    data =
      %{
        "aarch64-apple-darwin" => aarch64_apple_darwin,
        "x86_64-apple-darwin" => x86_64_apple_darwin,
        "aarch64-linux-gnu" => aarch64_linux_gnu,
        "x86_64-linux-gnu" => x86_64_linux_gnu,
        "x86_64-windows-msvc" => x86_64_windows_msvc
      }

    {driver, data}
  end

case String.split(File.read!(file), "# == CONSTANTS ==") do
  [pre, _mid, post] ->
    time =
      NaiveDateTime.utc_now() |> NaiveDateTime.truncate(:second) |> NaiveDateTime.to_iso8601()

    mid = """
    # == CONSTANTS ==

    # Generated by update.exs at #{time}. Do not change manually.
    @version #{inspect(version)}
    @driver_names #{inspect(drivers)}
    @driver_data #{inspect(mapping)}

    # == CONSTANTS ==
    """

    File.write!(file, Code.format_string!(pre <> mid <> post) <> "\n")

  _ ->
    raise "could not find # == CONSTANTS == chunks in #{file}"
end
