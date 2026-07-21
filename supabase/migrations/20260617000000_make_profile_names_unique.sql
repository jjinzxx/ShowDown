do $$
begin
  if not exists (
    select 1
    from pg_constraint
    where conname = 'profiles_username_key'
      and conrelid = 'public.profiles'::regclass
  ) then
    alter table public.profiles
      add constraint profiles_username_key unique (username);
  end if;

  if not exists (
    select 1
    from pg_constraint
    where conname = 'profiles_nickname_key'
      and conrelid = 'public.profiles'::regclass
  ) then
    alter table public.profiles
      add constraint profiles_nickname_key unique (nickname);
  end if;
end $$;
