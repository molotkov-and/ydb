(
    SELECT
        *
    FROM (
        VALUES
            (1),
            (2)
    ) AS t (
        x
    )
)
EXCEPT
(
    SELECT
        *
    FROM (
        VALUES
            (1)
    ) AS t (
        x
    )
)
EXCEPT
(
    SELECT
        *
    FROM (
        VALUES
            (1)
    ) AS t (
        x
    )
);
